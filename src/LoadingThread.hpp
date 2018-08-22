#pragma once

#include <thread>

class LoadingThread {
    bool running;
    std::thread thread;

    bool tick();

    void run();

public:

    LoadingThread() : running(false) {
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

