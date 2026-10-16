# Shared Memory Based Data Exchange System

A working Ubuntu/Linux hackathon prototype demonstrating inter-process communication (IPC) with System V shared memory and a POSIX process-shared semaphore.

## Features

- Separate command-line writer and reader processes.
- Shared message, version number, writer PID, and timestamp.
- `sem_t` initialized with `pshared=1` protects the shared data from concurrent reads/writes.
- Local Python web dashboard to view current data and write messages.
- Shared memory cleanup command.
- Test plan and presentation/report materials in `docs/`.

## Architecture

```text
Terminal Writer ─┐
                 ├── System V shared-memory segment
Local Dashboard ─┤   (message + version + metadata)
                 │             ▲
Terminal Reader ─┘       process-shared semaphore
```

The dashboard is a local UI: it invokes the C executable for reads/writes. The C executable is the component that actually accesses shared memory.

## Requirements

- Ubuntu/Linux
- GCC
- Python 3
- POSIX threads/semaphore support (provided by standard modern Linux systems)

Install build tools if needed:

```bash
sudo apt update
sudo apt install build-essential python3
```

## Compile and run

From the project directory:

```bash
gcc -Wall -Wextra -O2 shm_exchange.c -o shm_exchange -pthread
./shm_exchange init
```

### Terminal demo

Open separate terminals (or run sequentially):

```bash
./shm_exchange write "Hello from Process 1"
./shm_exchange read
./shm_exchange status
```

Write a second message and read again:

```bash
./shm_exchange write "Updated message from the writer"
./shm_exchange read
```

The version increases after each successful write.

### Dashboard demo

Keep the shared memory initialized. In a terminal:

```bash
python3 dashboard.py
```

Open Firefox/Chrome on the same machine and visit:

http://127.0.0.1:8000

Enter a message in the form. The dashboard writes it through the C program; the page refreshes every two seconds to show the current shared data. Keep it local; do not bind the server to `0.0.0.0` for this demo.

### Cleanup

Stop the dashboard with Ctrl+C, then run:

```bash
./shm_exchange cleanup
```

If the machine or program exits unexpectedly, the segment can remain until cleaned up. Run cleanup only when no other demo process is using the segment.

## Testing

See [`docs/TEST_PLAN.md`](docs/TEST_PLAN.md). Test the normal write/read path, version increments, long messages, dashboard writes, and concurrent writers. For concurrent testing, each writer's full message should remain intact; final order depends on scheduling.

## Project contents

- `shm_exchange.c` — shared-memory create/read/write/status/cleanup program.
- `dashboard.py` — local dashboard.
- `docs/PROJECT_REPORT.md` — report draft.
- `docs/PPT_OUTLINE.md` — slide-by-slide presentation outline.
- `docs/VIVA.md` — simple viva answers.
- `docs/TEST_PLAN.md` — demo test checklist.

## Limitations

- This is a teaching prototype with one fixed shared-memory key and one message slot, not a production message queue.
- Multiple writes are serialized; the most recent write replaces the previous message.
- A process killed while holding a semaphore can leave the lock unavailable. A production system should use robust process-shared mutexes or a recovery protocol.
- The segment is intended for local, controlled testing. Do not use sensitive data.
