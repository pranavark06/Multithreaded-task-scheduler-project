#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
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

class TaskScheduler {
public:
    static constexpr std::size_t kNumWorkers = 7;
    static constexpr std::size_t kNumShards = 8;

    TaskScheduler() {
        workers_.reserve(kNumWorkers);
        for (std::size_t i = 0; i < kNumWorkers; ++i) {
            workers_.emplace_back(&TaskScheduler::workerLoop, this);
        }
    }

    ~TaskScheduler() {
        shutdown();
    }

    void enqueue(Task task) {
        const std::size_t shard = task.id % kNumShards;
        {
            std::lock_guard<std::mutex> lock(queueMutexes_[shard]);
            taskQueues_[shard].push_back(std::move(task));
        }
        cv_.notify_one();
    }

    void enqueueBatch(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            Task task;
            task.id = i;
            task.delay = std::chrono::microseconds(250 + (i % 60) * 10);
            enqueue(std::move(task));
            {
                std::lock_guard<std::mutex> lock(totalMutex_);
                totalTasks_ += 1;
            }
        }
    }

    void waitUntilComplete() {
        std::unique_lock<std::mutex> lock(completedMutex_);
        completedCv_.wait(lock, [this] { return completedTasks_ >= totalTasks_; });
    }

    std::size_t completedTasks() const {
        return completedTasks_.load();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(cvMutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& thread : workers_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

private:
    std::optional<Task> popTask() {
        for (std::size_t shard = 0; shard < kNumShards; ++shard) {
            std::lock_guard<std::mutex> lock(queueMutexes_[shard]);
            if (!taskQueues_[shard].empty()) {
                Task task = std::move(taskQueues_[shard].front());
                taskQueues_[shard].pop_front();
                return task;
            }
        }
        return std::nullopt;
    }

    void workerLoop() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(cvMutex_);
                cv_.wait(lock, [this] { return stop_ || hasPendingWork(); });
                if (stop_ && !hasPendingWork()) {
                    return;
                }
            }

            auto task = popTask();
            if (!task.has_value()) {
                std::this_thread::yield();
                continue;
            }

            processTask(*task);
        }
    }

    void processTask(Task task) {
        {
            std::lock_guard<std::mutex> lock(task.state->mutex);
            task.state->started = true;
        }

        std::this_thread::sleep_for(task.delay);

        {
            std::lock_guard<std::mutex> lock(task.state->mutex);
            task.state->completed = true;
        }

        {
            std::lock_guard<std::mutex> lock(completedMutex_);
            completedTasks_ += 1;
        }
        completedCv_.notify_one();
    }

    bool hasPendingWork() const {
        for (std::size_t shard = 0; shard < kNumShards; ++shard) {
            if (!taskQueues_[shard].empty()) {
                return true;
            }
        }
        return false;
    }

    mutable std::array<std::deque<Task>, kNumShards> taskQueues_{};
    std::array<std::mutex, kNumShards> queueMutexes_{};
    std::mutex cvMutex_;
    std::condition_variable cv_;
    std::condition_variable completedCv_;
    std::mutex completedMutex_;
    std::mutex totalMutex_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> completedTasks_{0};
    std::size_t totalTasks_{0};
    bool stop_{false};
};

}  // namespace scheduler
