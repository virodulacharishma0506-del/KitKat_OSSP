#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

#define SHM_KEY 0x53484D31
#define MESSAGE_SIZE 512
#define MAGIC_VALUE 0x53484D58

typedef struct {
    int magic;
    sem_t mutex;
    unsigned long version;
    pid_t last_writer_pid;
    time_t updated_at;
    char message[MESSAGE_SIZE];
} SharedData;

static int get_segment(int create, int *created) {
    int flags = 0666 | (create ? IPC_CREAT : 0);
    int id = shmget((key_t)SHM_KEY, sizeof(SharedData), flags);
    if (id < 0) {
        perror("shmget (run './shm_exchange init' first)");
        return -1;
    }
    if (created) *created = 0;
    return id;
}

static SharedData *attach_segment(int id) {
    void *p = shmat(id, NULL, 0);
    if (p == (void *)-1) {
        perror("shmat");
        return NULL;
    }
    return (SharedData *)p;
}

static void detach_segment(SharedData *data) {
    if (data && shmdt(data) == -1) perror("shmdt");
}

static void print_time(time_t t) {
    if (t == 0) { printf("Never"); return; }
    char buf[64];
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    printf("%s", buf);
}

static int command_init(void) {
    int id = shmget((key_t)SHM_KEY, sizeof(SharedData), IPC_CREAT | IPC_EXCL | 0666);
    if (id < 0 && errno == EEXIST) {
        id = shmget((key_t)SHM_KEY, sizeof(SharedData), 0666);
        if (id < 0) { perror("shmget"); return 1; }
        SharedData *existing = attach_segment(id);
        if (!existing) return 1;
        if (existing->magic == MAGIC_VALUE) {
            printf("Shared memory is already initialized (ID %d).\n", id);
            detach_segment(existing);
            return 0;
        }
        detach_segment(existing);
        fprintf(stderr, "Segment exists but is not initialized. Run cleanup, then init.\n");
        return 1;
    }
    if (id < 0) { perror("shmget"); return 1; }

    SharedData *data = attach_segment(id);
    if (!data) { shmctl(id, IPC_RMID, NULL); return 1; }
    memset(data, 0, sizeof(*data));
    if (sem_init(&data->mutex, 1, 1) == -1) {
        perror("sem_init");
        detach_segment(data);
        shmctl(id, IPC_RMID, NULL);
        return 1;
    }
    data->magic = MAGIC_VALUE;
    data->version = 0;
    data->last_writer_pid = 0;
    data->updated_at = 0;
    snprintf(data->message, sizeof(data->message), "Welcome! Write a message to begin.");
    printf("Shared memory initialized successfully. Segment ID: %d\n", id);
    detach_segment(data);
    return 0;
}

static int lock_data(SharedData *data) {
    while (sem_wait(&data->mutex) == -1) {
        if (errno == EINTR) continue;
        perror("sem_wait");
        return -1;
    }
    return 0;
}

static int command_write(const char *message) {
    int id = get_segment(0, NULL);
    if (id < 0) return 1;
    SharedData *data = attach_segment(id);
    if (!data) return 1;
    if (data->magic != MAGIC_VALUE) {
        fprintf(stderr, "Shared memory is not initialized. Run './shm_exchange init'.\n");
        detach_segment(data); return 1;
    }
    if (lock_data(data) != 0) { detach_segment(data); return 1; }
    snprintf(data->message, sizeof(data->message), "%s", message);
    data->version++;
    data->last_writer_pid = getpid();
    data->updated_at = time(NULL);
    unsigned long version = data->version;
    if (sem_post(&data->mutex) == -1) perror("sem_post");
    printf("WRITE OK | version=%lu | pid=%ld | message=%s\n",
           version, (long)getpid(), data->message);
    detach_segment(data);
    return 0;
}

static int command_read(int json) {
    int id = get_segment(0, NULL);
    if (id < 0) return 1;
    SharedData *data = attach_segment(id);
    if (!data) return 1;
    if (data->magic != MAGIC_VALUE) {
        fprintf(stderr, "Shared memory is not initialized. Run './shm_exchange init'.\n");
        detach_segment(data); return 1;
    }
    if (lock_data(data) != 0) { detach_segment(data); return 1; }
    char message[MESSAGE_SIZE];
    snprintf(message, sizeof(message), "%s", data->message);
    unsigned long version = data->version;
    pid_t pid = data->last_writer_pid;
    time_t updated = data->updated_at;
    if (sem_post(&data->mutex) == -1) perror("sem_post");

    if (json) {
        printf("{\"message\":\"");
        for (const char *p = message; *p; ++p) {
            if (*p == '"' || *p == '\\') putchar('\\');
            if (*p == '\n') { printf("\\n"); continue; }
            if ((unsigned char)*p >= 32) putchar(*p);
        }
        printf("\",\"version\":%lu,\"writer_pid\":%ld,\"updated_at\":%ld}\n",
               version, (long)pid, (long)updated);
    } else {
        printf("READ OK | version=%lu | last_writer_pid=%ld | updated_at=",
               version, (long)pid);
        print_time(updated);
        printf("\nMESSAGE: %s\n", message);
    }
    detach_segment(data);
    return 0;
}

static int command_status(void) { return command_read(1); }

static int command_cleanup(void) {
    int id = shmget((key_t)SHM_KEY, sizeof(SharedData), 0666);
    if (id < 0) {
        if (errno == ENOENT) { printf("No shared-memory segment exists.\n"); return 0; }
        perror("shmget"); return 1;
    }
    SharedData *data = attach_segment(id);
    if (data) {
        if (data->magic == MAGIC_VALUE) sem_destroy(&data->mutex);
        detach_segment(data);
    }
    if (shmctl(id, IPC_RMID, NULL) == -1) { perror("shmctl IPC_RMID"); return 1; }
    printf("Shared memory removed successfully.\n");
    return 0;
}

static void usage(const char *prog) {
    printf("Usage:\n"
           "  %s init\n"
           "  %s write <message>\n"
           "  %s read\n"
           "  %s status\n"
           "  %s cleanup\n", prog, prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    if (strcmp(argv[1], "init") == 0) return command_init();
    if (strcmp(argv[1], "write") == 0) {
        if (argc < 3) { fprintf(stderr, "Please provide a message.\n"); return 1; }
        char message[MESSAGE_SIZE] = "";
        for (int i = 2; i < argc; ++i) {
            if (i > 2) strncat(message, " ", sizeof(message)-strlen(message)-1);
            strncat(message, argv[i], sizeof(message)-strlen(message)-1);
        }
        return command_write(message);
    }
    if (strcmp(argv[1], "read") == 0) return command_read(0);
    if (strcmp(argv[1], "status") == 0) return command_status();
    if (strcmp(argv[1], "cleanup") == 0) return command_cleanup();
    usage(argv[0]);
    return 1;
}
