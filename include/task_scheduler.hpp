# Multithreaded Task Scheduler

This project implements a Linux-friendly multithreaded task scheduler that uses:

- 7 worker threads
- fine-grained locking per queue shard
- task-level mutexes for task status tracking
- a queue of 7,500 tasks

The scheduler sharded the work queue into 8 buckets, each with its own mutex. This avoids a single global lock becoming the bottleneck and keeps the system responsive under load.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/task_scheduler
```

Expected output:

```text
Completed 7500 tasks using 7 worker threads.
```

## Notes

The implementation intentionally uses fine-grained locking instead of a single global mutex. The per-shard queue locks eliminate contention for queue access, while each task has its own mutex to guard its started/completed state.

This makes the scheduler suitable for a Linux environment and demonstrates the concurrency pattern needed for a multithreaded task dispatcher.

