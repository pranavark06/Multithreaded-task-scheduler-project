# Multithreaded Task Scheduler

A Linux-friendly C++17 worker pool with dependency-aware task submission. Tasks can form a directed acyclic graph (DAG): independent tasks run in parallel, and each task becomes ready only after all of its prerequisites finish. This supports fork/join workloads without blocking worker threads while waiting on dependencies.

## Features

- 7 worker threads and a thread-safe ready queue.
- `submit(callable, dependencies)` returns a `TaskHandle` with `wait()` and `get()`.
- Dependencies can be shared across multiple tasks; cross-scheduler and empty dependency handles are rejected.
- Task exceptions are rethrown by the corresponding handle's `get()`. Dependents are still released after a failed task, so applications can decide how to handle failures.
- `enqueue` and `enqueueBatch` remain available for the original delay-based example.
- `waitUntilComplete()` waits until all submitted tasks, including dependency-blocked tasks, finish.

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/task_scheduler
```

The example builds a fork/join graph: one load task feeds two parallel tasks, which both feed a final publish task.

## Example API

```cpp
scheduler::TaskScheduler pool;
auto load = pool.submit([] { /* load data */ });
auto left = pool.submit([] { /* process left side */ }, {load});
auto right = pool.submit([] { /* process right side */ }, {load});
auto merge = pool.submit([] { /* combine results */ }, {left, right});
merge.get();
```

Each task's callable runs once. A task's dependencies must be handles returned by the same scheduler; graph cycles cannot be introduced because dependencies may only refer to already-submitted tasks.
