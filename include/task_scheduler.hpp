#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace scheduler {

struct TaskState {
    std::mutex mutex;
    bool started = false;
    bool completed = false;
};

struct Task {
    std::size_t id = 0;
    std::chrono::microseconds delay{0};
    std::shared_ptr<TaskState> state = std::make_shared<TaskState>();
};

namespace detail {
struct SchedulerToken {};

struct TaskNode {
    explicit TaskNode(std::function<void()> function,
                      const std::shared_ptr<SchedulerToken>& scheduler)
        : function(std::move(function)), owner(scheduler), result(promise.get_future().share()) {}

    std::function<void()> function;
    std::weak_ptr<SchedulerToken> owner;
    std::promise<void> promise;
    std::shared_future<void> result;
    std::vector<std::weak_ptr<TaskNode>> dependents;
    std::size_t dependenciesRemaining = 0;
    bool completed = false;
};
}  // namespace detail

// A copyable handle to a scheduled task. Calling get() rethrows exceptions
// produced by the task; dependent tasks are still released after a failure.
class TaskHandle {
public:
    TaskHandle() = default;

    bool valid() const noexcept { return static_cast<bool>(node_); }
    void wait() const { result_.wait(); }
    void get() const { result_.get(); }

private:
    friend class TaskScheduler;
    explicit TaskHandle(std::shared_ptr<detail::TaskNode> node)
        : node_(std::move(node)), result_(node_->result) {}

    std::shared_ptr<detail::TaskNode> node_;
    std::shared_future<void> result_;
};

// A fixed-size worker pool with dependency-aware submission. A task becomes
// runnable only after every task in its dependency list has finished.
class TaskScheduler {
public:
    static constexpr std::size_t kNumWorkers = 7;
    static constexpr std::size_t kNumShards = 8;  // Kept for source compatibility.

    TaskScheduler() : token_(std::make_shared<detail::SchedulerToken>()) {
        workers_.reserve(kNumWorkers);
        for (std::size_t i = 0; i < kNumWorkers; ++i) {
            workers_.emplace_back(&TaskScheduler::workerLoop, this);
        }
    }

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    ~TaskScheduler() { shutdown(); }

    TaskHandle submit(std::function<void()> function,
                      const std::vector<TaskHandle>& dependencies = {}) {
        if (!function) {
            throw std::invalid_argument("submitted task must be callable");
        }

        auto node = std::make_shared<detail::TaskNode>(std::move(function), token_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("cannot submit work after scheduler shutdown");
            }

            for (const auto& dependency : dependencies) {
                if (!dependency.node_) {
                    throw std::invalid_argument("dependency handle is empty");
                }
                if (dependency.node_->owner.lock() != token_) {
                    throw std::invalid_argument("dependency belongs to another scheduler");
                }
                if (!dependency.node_->completed) {
                    ++node->dependenciesRemaining;
                    dependency.node_->dependents.emplace_back(node);
                }
            }

            ++outstanding_;
            if (node->dependenciesRemaining == 0) {
                ready_.emplace_back(node);
            }
        }
        workAvailable_.notify_one();
        return TaskHandle(std::move(node));
    }

    // Compatibility helper for the original delay-based example.
    void enqueue(Task task) {
        const auto state = task.state;
        const auto delay = task.delay;
        submit([state, delay] {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->started = true;
            }
            std::this_thread::sleep_for(delay);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->completed = true;
            }
        });
    }

    void enqueueBatch(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            Task task;
            task.id = i;
            task.delay = std::chrono::microseconds(250 + (i % 60) * 10);
            enqueue(std::move(task));
        }
    }

    // Waits for all work submitted before the scheduler becomes idle. Tasks
    // submitted concurrently while this waits are included in the idle check.
    void waitUntilComplete() {
        std::unique_lock<std::mutex> lock(mutex_);
        idle_.wait(lock, [this] { return outstanding_ == 0; });
    }

    std::size_t completedTasks() const { return completedTasks_.load(); }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        workAvailable_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    void workerLoop() {
        while (true) {
            std::shared_ptr<detail::TaskNode> node;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                workAvailable_.wait(lock, [this] { return stopping_ || !ready_.empty(); });
                if (ready_.empty()) {
                    if (stopping_) {
                        return;
                    }
                    continue;
                }
                node = std::move(ready_.front());
                ready_.pop_front();
            }

            std::exception_ptr failure;
            try {
                node->function();
            } catch (...) {
                failure = std::current_exception();
            }

            // Fulfill the handle before publishing completion to dependents.
            if (failure) {
                node->promise.set_exception(failure);
            } else {
                node->promise.set_value();
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                node->completed = true;
                for (const auto& weakDependent : node->dependents) {
                    if (auto dependent = weakDependent.lock()) {
                        if (--dependent->dependenciesRemaining == 0) {
                            ready_.emplace_back(std::move(dependent));
                        }
                    }
                }
                node->dependents.clear();
                --outstanding_;
                ++completedTasks_;
                if (outstanding_ == 0) {
                    idle_.notify_all();
                }
            }
            workAvailable_.notify_all();
        }
    }

    const std::shared_ptr<detail::SchedulerToken> token_;
    std::deque<std::shared_ptr<detail::TaskNode>> ready_;
    std::mutex mutex_;
    std::condition_variable workAvailable_;
    std::condition_variable idle_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> completedTasks_{0};
    std::size_t outstanding_ = 0;
    bool stopping_ = false;
};

}  // namespace scheduler
