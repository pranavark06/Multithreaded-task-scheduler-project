#include "task_scheduler.hpp"

#include <iostream>

int main() {
    scheduler::TaskScheduler scheduler;

    const std::size_t taskCount = 7500;
    scheduler.enqueueBatch(taskCount);
    scheduler.waitUntilComplete();

    std::cout << "Completed " << scheduler.completedTasks() << " tasks using "
              << scheduler::TaskScheduler::kNumWorkers << " worker threads.\n";
    return 0;
}
