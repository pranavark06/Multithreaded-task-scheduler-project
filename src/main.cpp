#include "task_scheduler.hpp"

#include <iostream>
#include <vector>

int main() {
    scheduler::TaskScheduler scheduler;

    // The second task cannot start until the first finishes. The third waits
    // for both, demonstrating a fork/join dependency graph.
    const auto load = scheduler.submit([] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
    const auto parse = scheduler.submit([] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }, {load});
    const auto index = scheduler.submit([] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }, {load});
    const auto publish = scheduler.submit([] { std::cout << "Dependency graph completed.\n"; }, {parse, index});

    publish.get();
    scheduler.waitUntilComplete();
    std::cout << "Completed " << scheduler.completedTasks() << " tasks using "
              << scheduler::TaskScheduler::kNumWorkers << " worker threads.\n";
    return 0;
}
