#pragma once

#include <TaskScheduler.h>

// This creates a singleton wrapper around the TaskScheduler
// to ensure we only have one instance and one include of the full implementation
class TaskManager {
public:
    static TaskManager& getInstance() {
        static TaskManager instance;
        return instance;
    }

    void init() {
        scheduler.init();
    }

    void execute() {
        scheduler.execute();
    }

    Scheduler& getScheduler() {
        return scheduler;
    }

private:
    TaskManager() {}
    ~TaskManager() {}
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    
    Scheduler scheduler;
}; 