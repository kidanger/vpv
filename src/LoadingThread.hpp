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
    std::function<std::shared_ptr<Progressable>()> getnew;

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

#include <mutex>
#include <condition_variable>

class SleepyLoadingThread {
    bool running;
    std::thread thread;
    std::queue<std::shared_ptr<Progressable>> queue;
    std::function<std::shared_ptr<Progressable>()> getnew;
    std::mutex m;
    std::condition_variable cv;
    bool ready;

    bool tick();

    void run();

public:

    SleepyLoadingThread(std::function<std::shared_ptr<Progressable>()> getnew)
        : running(false), getnew(getnew), ready(false) {
    }

    void start() {
        running = true;
        thread = std::thread(&SleepyLoadingThread::run, this);
    }

    void stop() {
        running = false;
        notify();
    }

    void join() {
        thread.join();
    }

    void notify() {
        {
            std::lock_guard<std::mutex> lk(m);
            ready = true;
        }
        cv.notify_one();
    }

};


