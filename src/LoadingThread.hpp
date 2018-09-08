#pragma once

#include <thread>
#include <queue>
#include <memory>
#include <functional>

class Progressable;

class LoadingThread {
    bool running;
    std::thread thread;
    std::queue<std::shared_ptr<Progressable>> queue;
    const std::function<std::shared_ptr<Progressable>()>& getnew;

    bool tick();

    void run();

public:

    LoadingThread(std::function<std::shared_ptr<Progressable>()> getnew)
        : running(false), getnew(getnew) {
    }

    void start() {
        running = true;
        thread = std::thread(&LoadingThread::run, this);
    }

    void stop() {
        running = false;
    }

    void join() {
        thread.join();
    }
};

