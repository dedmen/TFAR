#pragma once
#include <vector>
#include <functional>
#include <mutex>
#include "MainthreadTester.hpp"

static inline __itt_domain* MainthreadSchedulerDomain = __itt_domain_create("MainthreadScheduler");

static inline __itt_string_handle* MainthreadScheduler_pushTask = __itt_string_handle_create("pushTask");
static inline __itt_string_handle* MainthreadScheduler_executeTasks = __itt_string_handle_create("executeTasks");


class MainthreadScheduler {
public:

    void pushTask(std::function<void()>&& newTask) {
        ittScope sc(MainthreadSchedulerDomain, MainthreadScheduler_pushTask);
        std::unique_lock lock(accessMutex);
        tasks.emplace_back(std::move(newTask));
    }
    void executeTasks() {
        ittScope sc(MainthreadSchedulerDomain, MainthreadScheduler_executeTasks);
        MainthreadTester::checkNow();
        std::unique_lock lock(accessMutex);
        auto taskMove = std::move(tasks); //Executing tasks might push more tasks which might clear memory
        lock.unlock();//We moved and cleared tasks, don't need anymore

        for (auto& it : taskMove) {
            it();
        }
    }
private:
    std::recursive_mutex accessMutex;
    std::vector<std::function<void()>> tasks;
};
