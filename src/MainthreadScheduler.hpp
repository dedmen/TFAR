#pragma once
#include <vector>
#include <functional>
#include <mutex>
#include "MainthreadTester.hpp"

class MainthreadScheduler {
public:

    void pushTask(std::function<void()>&& newTask) {
        std::unique_lock<std::mutex> lock(accessMutex);
        tasks.emplace_back(std::move(newTask));
    }
    void executeTasks() {
        MainthreadTester::checkNow();
        std::unique_lock<std::mutex> lock(accessMutex);
        for (auto& it : tasks)
            it();
        tasks.clear();
    }
private:
    std::mutex accessMutex;
    std::vector<std::function<void()>> tasks;
};
