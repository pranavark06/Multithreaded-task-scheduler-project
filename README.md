# Multithreaded Task Scheduler

A Linux-friendly C++17 task scheduler with dependency-aware submission, plus a command-line tool that recommends which of your tasks to complete first based on urgency.

## Prioritize your tasks

Build and run the program:

```bash
cmake -S . -B build
cmake --build build
./build/task_scheduler
```

Enter the number of tasks, then each task's name and urgency from **1** (least urgent) to **5** (most urgent). The program recommends the highest-urgency task and prints the full priority order. If tasks have the same urgency, their original input order is preserved.

Example:

```text
How many tasks do you want to prioritize? 3
Task 1 name: Finish report
Urgency for "Finish report" (1-5, 5 = most urgent): 3
Task 2 name: Submit application
Urgency for "Submit application" (1-5, 5 = most urgent): 5
Task 3 name: Buy groceries
Urgency for "Buy groceries" (1-5, 5 = most urgent): 2

Recommended task to complete first: Submit application (urgency 5/5)
```

## Scheduler API

The `scheduler::TaskScheduler` library provides a fixed-size worker pool and dependency-aware task submission. Tasks can form a directed acyclic graph: independent tasks run in parallel, and each task becomes ready only after all of its dependencies finish.

```cpp
scheduler::TaskScheduler pool;
auto load = pool.submit([] { /* load data */ });
auto left = pool.submit([] { /* process left side */ }, {load});
auto right = pool.submit([] { /* process right side */ }, {load});
auto merge = pool.submit([] { /* combine results */ }, {left, right});
merge.get();
```

`submit(callable, dependencies)` returns a `TaskHandle` with `wait()` and `get()`. Dependencies must be handles returned by the same scheduler. Task exceptions are rethrown by the corresponding handle's `get()`; dependents are still released after a failure. `enqueue`, `enqueueBatch`, `waitUntilComplete()`, and `completedTasks()` are also available.
