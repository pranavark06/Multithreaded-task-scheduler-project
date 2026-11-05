#include "task_scheduler.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
struct UserTask {
    std::string name;
    int urgency;
};

bool readTaskCount(std::size_t& count) {
    long long input = 0;
    std::cout << "How many tasks do you want to prioritize? ";
    if (!(std::cin >> input) || input <= 0) {
        std::cerr << "Please enter a positive whole number of tasks.\n";
        return false;
    }
    count = static_cast<std::size_t>(input);
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return true;
}
}  // namespace

int main() {
    std::size_t taskCount = 0;
    if (!readTaskCount(taskCount)) {
        return 1;
    }

    std::vector<UserTask> tasks;
    tasks.reserve(taskCount);

    for (std::size_t i = 0; i < taskCount; ++i) {
        std::string name;
        std::cout << "Task " << (i + 1) << " name: ";
        if (!std::getline(std::cin, name) || name.find_first_not_of(" \t\r\n") == std::string::npos) {
            std::cerr << "Task names cannot be empty.\n";
            return 1;
        }

        int urgency = 0;
        while (true) {
            std::cout << "Urgency for \"" << name << "\" (1-5, 5 = most urgent): ";
            if (std::cin >> urgency && urgency >= 1 && urgency <= 5) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                break;
            }

            if (std::cin.eof()) {
                std::cerr << "Input ended before all tasks were entered.\n";
                return 1;
            }
            std::cout << "Enter a whole number from 1 to 5.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        tasks.push_back({std::move(name), urgency});
    }

    // stable_sort preserves the user's input order when urgency levels tie.
    std::stable_sort(tasks.begin(), tasks.end(), [](const UserTask& left, const UserTask& right) {
        return left.urgency > right.urgency;
    });

    std::cout << "\nRecommended task to complete first: " << tasks.front().name
              << " (urgency " << tasks.front().urgency << "/5)\n";
    std::cout << "\nFull priority order:\n";
    for (std::size_t i = 0; i < tasks.size(); ++i) {
        std::cout << (i + 1) << ". " << tasks[i].name
                  << " — urgency " << tasks[i].urgency << "/5\n";
    }
    return 0;
}
