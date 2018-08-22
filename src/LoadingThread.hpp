#pragma once

#include <thread>
#include <queue>
#include <memory>

class ImageProvider;

class LoadingThread {
    bool running;
    std::thread thread;
    std::queue<std::shared_ptr<ImageProvider>> queue;

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

