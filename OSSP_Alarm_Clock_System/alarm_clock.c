/*
============================================================
        SMART ALARM CLOCK SYSTEM
        Linux / Ubuntu OSSP Project
============================================================

OS Concepts Used:
1. fork()              - Process creation
2. mmap()              - Shared memory
3. signal()            - Signal handling
4. kill()              - Process communication
5. time()              - System time
6. File handling       - Alarm history
7. sleep()             - Process timing

Features:
- Multiple alarms
- Alarm labels
- Once / Daily / Weekday alarms
- Enable / Disable
- Delete alarm
- Snooze
- Alarm history
- Countdown timer
- 12/24 hour display
- Alarm monitor child process
- No large switch-case
============================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define MAX_ALARMS 5
#define LABEL_SIZE 50

/* Repeat types */
#define ONCE 1
#define DAILY 2
#define WEEKDAYS 3

/* Alarm structure */
typedef struct
{
    int hour;
    int minute;

    char label[LABEL_SIZE];

    int active;
    int repeat_type;

    int triggered;
} Alarm;


/* Shared memory structure */
typedef struct
{
    Alarm alarms[MAX_ALARMS];

    int display_24_hour;

    int alarm_event;
    int event_alarm;

    int running;

} SharedData;


/* Global shared memory pointer */
SharedData *data;

/* Child process ID */
pid_t monitor_pid;

/* Signal flag */
volatile sig_atomic_t alarm_received = 0;


/* ==========================================================
   Signal Handler
   ========================================================== */

void alarm_signal_handler(int signal_number)
{
    alarm_received = 1;
}


/* ==========================================================
   Initialize shared memory
   ========================================================== */

void initialize_data()
{
    int i;

    data->display_24_hour = 0;
    data->alarm_event = 0;
    data->event_alarm = -1;
    data->running = 1;

    for (i = 0; i < MAX_ALARMS; i++)
    {
        data->alarms[i].hour = -1;
        data->alarms[i].minute = -1;

        data->alarms[i].label[0] = '\0';

        data->alarms[i].active = 0;
        data->alarms[i].repeat_type = ONCE;
        data->alarms[i].triggered = 0;
    }
}


/* ==========================================================
   Display Time
   ========================================================== */

void display_current_time()
{
    time_t now;
    struct tm *current;

    time(&now);
    current = localtime(&now);

    int hour = current->tm_hour;
    int minute = current->tm_min;
    int second = current->tm_sec;

    printf("\n--------------------------------------\n");
    printf("           CURRENT TIME\n");
    printf("--------------------------------------\n");

    if (data->display_24_hour)
    {
        printf("%02d:%02d:%02d\n",
               hour,
               minute,
               second);
    }
    else
    {
        char *period;

        if (hour >= 12)
            period = "PM";
        else
            period = "AM";

        int display_hour = hour % 12;

        if (display_hour == 0)
            display_hour = 12;

        printf("%02d:%02d:%02d %s\n",
               display_hour,
               minute,
               second,
               period);
    }

    printf("--------------------------------------\n");
}


/* ==========================================================
   Repeat Name
   ========================================================== */

void print_repeat_type(int type)
{
    if (type == ONCE)
        printf("Once");

    else if (type == DAILY)
        printf("Daily");

    else if (type == WEEKDAYS)
        printf("Weekdays");
}


/* ==========================================================
   Set Alarm
   ========================================================== */

void set_alarm()
{
    int index = -1;
    int hour;
    int minute;
    int repeat;
    int i;

    for (i = 0; i < MAX_ALARMS; i++)
    {
        if (!data->alarms[i].active &&
            data->alarms[i].hour == -1)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        printf("\nAll alarm slots are occupied.\n");
        return;
    }

    printf("\n========== CREATE ALARM ==========\n");

    printf("Enter hour (0-23): ");
    scanf("%d", &hour);

    printf("Enter minute (0-59): ");
    scanf("%d", &minute);

    if (hour < 0 || hour > 23 ||
        minute < 0 || minute > 59)
    {
        printf("\nInvalid time.\n");
        return;
    }

    getchar();

    printf("Enter alarm label: ");

    fgets(data->alarms[index].label,
          LABEL_SIZE,
          stdin);

    data->alarms[index].label[
        strcspn(data->alarms[index].label, "\n")
    ] = '\0';


    printf("\nRepeat options:\n");
    printf("1. Once\n");
    printf("2. Daily\n");
    printf("3. Weekdays\n");

    printf("Choose repeat type: ");
    scanf("%d", &repeat);

    if (repeat < 1 || repeat > 3)
    {
        printf("\nInvalid repeat type.\n");
        return;
    }

    data->alarms[index].hour = hour;
    data->alarms[index].minute = minute;

    data->alarms[index].active = 1;
    data->alarms[index].repeat_type = repeat;
    data->alarms[index].triggered = 0;

    printf("\n=================================\n");
    printf("Alarm created successfully!\n");
    printf("Alarm ID : %d\n", index + 1);
    printf("Time     : %02d:%02d\n",
           hour,
           minute);

    printf("Label    : %s\n",
           data->alarms[index].label);

    printf("Repeat   : ");
    print_repeat_type(repeat);

    printf("\n=================================\n");
}


/* ==========================================================
   View Alarms
   ========================================================== */

void view_alarms()
{
    int i;
    int found = 0;

    printf("\n========== YOUR ALARMS ==========\n");

    for (i = 0; i < MAX_ALARMS; i++)
    {
        if (data->alarms[i].hour != -1)
        {
            found = 1;

            printf("\nAlarm ID : %d\n", i + 1);

            printf("Time     : %02d:%02d\n",
                   data->alarms[i].hour,
                   data->alarms[i].minute);

            printf("Label    : %s\n",
                   data->alarms[i].label);

            printf("Repeat   : ");

            print_repeat_type(
                data->alarms[i].repeat_type
            );

            printf("\n");

            if (data->alarms[i].active)
                printf("Status   : Enabled\n");
            else
                printf("Status   : Disabled\n");

            printf("---------------------------------\n");
        }
    }

    if (!found)
        printf("\nNo alarms available.\n");
}


/* ==========================================================
   Delete Alarm
   ========================================================== */

void delete_alarm()
{
    int number;

    view_alarms();

    printf("\nEnter alarm ID to delete: ");
    scanf("%d", &number);

    if (number < 1 || number > MAX_ALARMS)
    {
        printf("\nInvalid alarm ID.\n");
        return;
    }

    int index = number - 1;

    if (data->alarms[index].hour == -1)
    {
        printf("\nAlarm does not exist.\n");
        return;
    }

    data->alarms[index].hour = -1;
    data->alarms[index].minute = -1;

    data->alarms[index].label[0] = '\0';

    data->alarms[index].active = 0;
    data->alarms[index].triggered = 0;

    printf("\nAlarm deleted successfully.\n");
}


/* ==========================================================
   Enable / Disable
   ========================================================== */

void toggle_alarm()
{
    int number;

    view_alarms();

    printf("\nEnter alarm ID: ");
    scanf("%d", &number);

    if (number < 1 || number > MAX_ALARMS)
    {
        printf("\nInvalid alarm ID.\n");
        return;
    }

    int index = number - 1;

    if (data->alarms[index].hour == -1)
    {
        printf("\nAlarm does not exist.\n");
        return;
    }

    if (data->alarms[index].active)
    {
        data->alarms[index].active = 0;

        printf("\nAlarm disabled.\n");
    }
    else
    {
        data->alarms[index].active = 1;
        data->alarms[index].triggered = 0;

        printf("\nAlarm enabled.\n");
    }
}


/* ==========================================================
   Snooze Alarm
   ========================================================== */

void snooze_alarm(int index)
{
    if (index < 0 || index >= MAX_ALARMS)
        return;

    data->alarms[index].minute += 5;

    if (data->alarms[index].minute >= 60)
    {
        data->alarms[index].minute -= 60;

        data->alarms[index].hour++;

        if (data->alarms[index].hour >= 24)
            data->alarms[index].hour = 0;
    }

    data->alarms[index].active = 1;
    data->alarms[index].triggered = 0;

    printf("\nAlarm snoozed for 5 minutes.\n");

    printf("Next alarm: %02d:%02d\n",
           data->alarms[index].hour,
           data->alarms[index].minute);
}


/* ==========================================================
   Alarm History
   ========================================================== */

void save_history(int index)
{
    FILE *file;

    file = fopen("alarm_history.txt", "a");

    if (file == NULL)
    {
        printf("Unable to open history file.\n");
        return;
    }

    time_t now;
    struct tm *current;

    time(&now);
    current = localtime(&now);

    fprintf(file,
            "%02d-%02d-%04d %02d:%02d - %s - Triggered\n",
            current->tm_mday,
            current->tm_mon + 1,
            current->tm_year + 1900,
            current->tm_hour,
            current->tm_min,
            data->alarms[index].label);

    fclose(file);
}


/* ==========================================================
   View History
   ========================================================== */

void view_history()
{
    FILE *file;
    char line[200];

    file = fopen("alarm_history.txt", "r");

    if (file == NULL)
    {
        printf("\nNo alarm history available.\n");
        return;
    }

    printf("\n========== ALARM HISTORY ==========\n");

    while (fgets(line, sizeof(line), file))
    {
        printf("%s", line);
    }

    fclose(file);

    printf("\n===================================\n");
}


/* ==========================================================
   Timer
   ========================================================== */

void start_timer()
{
    int seconds;

    printf("\n========== COUNTDOWN TIMER ==========\n");

    printf("Enter timer duration in seconds: ");
    scanf("%d", &seconds);

    if (seconds <= 0)
    {
        printf("\nInvalid duration.\n");
        return;
    }

    printf("\nTimer started...\n");

    while (seconds > 0)
    {
        printf("\rRemaining: %d seconds   ",
               seconds);

        fflush(stdout);

        sleep(1);

        seconds--;
    }

    printf("\n\n");
    printf("*********************************\n");
    printf("*       TIMER COMPLETED!        *\n");
    printf("*********************************\n");

    printf("\a");
}


/* ==========================================================
   Settings
   ========================================================== */

void settings()
{
    int choice;

    printf("\n========== SETTINGS ==========\n");

    printf("1. 12-hour format\n");
    printf("2. 24-hour format\n");

    printf("Choose format: ");
    scanf("%d", &choice);

    if (choice == 1)
    {
        data->display_24_hour = 0;

        printf("\n12-hour format enabled.\n");
    }

    else if (choice == 2)
    {
        data->display_24_hour = 1;

        printf("\n24-hour format enabled.\n");
    }

    else
    {
        printf("\nInvalid option.\n");
    }
}


/* ==========================================================
   Alarm Monitor
   ========================================================== */

void alarm_monitor()
{
    while (data->running)
    {
        time_t now;
        struct tm *current;

        time(&now);

        current = localtime(&now);

        int hour = current->tm_hour;
        int minute = current->tm_min;

        int weekday = current->tm_wday;

        int i;

        for (i = 0; i < MAX_ALARMS; i++)
        {
            Alarm *a = &data->alarms[i];

            if (!a->active)
                continue;

            if (a->hour != hour ||
                a->minute != minute)
                continue;

            if (a->triggered)
                continue;

            /*
             * Weekday alarm:
             * Sunday = 0
             * Saturday = 6
             */
            if (a->repeat_type == WEEKDAYS)
            {
                if (weekday == 0 || weekday == 6)
                    continue;
            }

            a->triggered = 1;

            data->alarm_event = 1;
            data->event_alarm = i;

            save_history(i);

            /*
             * Notify parent process
             */
            kill(getppid(), SIGUSR1);

            /*
             * For one-time alarms,
             * disable after triggering.
             */
            if (a->repeat_type == ONCE)
            {
                a->active = 0;
            }
        }

        /*
         * Reset daily alarm trigger
         * when the minute changes.
         */
        for (i = 0; i < MAX_ALARMS; i++)
        {
            if (data->alarms[i].repeat_type != ONCE)
            {
                if (data->alarms[i].minute != minute)
                {
                    data->alarms[i].triggered = 0;
                }
            }
        }

        sleep(1);
    }
}


/* ==========================================================
   Display Alarm Event
   ========================================================== */

void process_alarm_event()
{
    if (!data->alarm_event)
        return;

    int index = data->event_alarm;

    if (index < 0 || index >= MAX_ALARMS)
    {
        data->alarm_event = 0;
        return;
    }

    printf("\n\n");
    printf("****************************************\n");
    printf("*           🔔 ALARM RINGING!          *\n");
    printf("****************************************\n");

    printf("\nLabel : %s\n",
           data->alarms[index].label);

    printf("Time  : %02d:%02d\n",
           data->alarms[index].hour,
           data->alarms[index].minute);

    printf("\nWake Up! Time for your activity!\n");

    printf("\a\a\a\a\a");

    printf("\n");

    if (data->alarms[index].repeat_type != ONCE)
    {
        printf("This alarm is a recurring alarm.\n");
    }

    printf("****************************************\n");

    data->alarm_event = 0;
    data->event_alarm = -1;
}


/* ==========================================================
   Main Menu
   ========================================================== */

void show_menu()
{
    printf("\n\n");
    printf("============================================\n");
    printf("           SMART ALARM CLOCK\n");
    printf("============================================\n");

    printf("1.  Set New Alarm\n");
    printf("2.  View Current Time\n");
    printf("3.  View All Alarms\n");
    printf("4.  Enable / Disable Alarm\n");
    printf("5.  Delete Alarm\n");
    printf("6.  View Alarm History\n");
    printf("7.  Countdown Timer\n");
    printf("8.  Settings\n");
    printf("9.  Exit\n");

    printf("============================================\n");
    printf("Enter your choice: ");
}


/* ==========================================================
   Main
   ========================================================== */

int main()
{
    int choice;

    /*
     * Create shared memory.
     *
     * MAP_SHARED means both parent
     * and child can access the same data.
     */

    data = mmap(
        NULL,
        sizeof(SharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    if (data == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }


    initialize_data();


    /*
     * Register signal handler
     */
    signal(SIGUSR1, alarm_signal_handler);


    /*
     * Create child process
     */
    monitor_pid = fork();


    if (monitor_pid < 0)
    {
        perror("fork failed");

        munmap(data, sizeof(SharedData));

        return 1;
    }


    /*
     * Child process
     */
    if (monitor_pid == 0)
    {
        printf("\n[Alarm Monitor Process Started]\n");

        alarm_monitor();

        exit(0);
    }


    /*
     * Parent process
     */

    printf("\n");
    printf("============================================\n");
    printf("      SMART ALARM CLOCK SYSTEM STARTED\n");
    printf("============================================\n");

    printf("Main Process PID    : %d\n",
           getpid());

    printf("Monitor Process PID : %d\n",
           monitor_pid);

    printf("Shared Memory       : Active\n");

    printf("============================================\n");


    while (data->running)
    {
        /*
         * Check if child generated
         * an alarm event.
         */
        if (alarm_received)
        {
            alarm_received = 0;

            process_alarm_event();
        }


        show_menu();

        scanf("%d", &choice);


        /*
         * Menu implemented using
         * if-else instead of switch.
         */

        if (choice == 1)
        {
            set_alarm();
        }

        else if (choice == 2)
        {
            display_current_time();
        }

        else if (choice == 3)
        {
            view_alarms();
        }

        else if (choice == 4)
        {
            toggle_alarm();
        }

        else if (choice == 5)
        {
            delete_alarm();
        }

        else if (choice == 6)
        {
            view_history();
        }

        else if (choice == 7)
        {
            start_timer();
        }

        else if (choice == 8)
        {
            settings();
        }

        else if (choice == 9)
        {
            printf("\nStopping Alarm Clock System...\n");

            data->running = 0;

            /*
             * Tell child process to stop.
             */
            kill(monitor_pid, SIGTERM);

            printf("Alarm Monitor stopped.\n");

            break;
        }

        else
        {
            printf("\nInvalid choice.\n");
        }
    }


    /*
     * Release shared memory
     */

    munmap(
        data,
        sizeof(SharedData)
    );

    printf("\nThank you for using Smart Alarm Clock!\n");

    return 0;
}