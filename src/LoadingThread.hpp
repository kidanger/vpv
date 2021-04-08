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

#include "globals.hpp"

template<typename T>
class SleepyLoadingThread {
    bool running;
    std::thread thread;
    std::shared_ptr<T> current;
    std::function<std::shared_ptr<T>()> getnew;
    std::mutex mutex;
    std::condition_variable cv;
    bool ready;

    bool tick()
    {
        // load the queue
        if (current) {
            current->progress();
            // if the provider is used somewhere else, refresh the screen
            if (current.use_count() != 1) {
                gActive = std::max(gActive, 2);
            }
            if (current->isLoaded()) {
                current = nullptr;
            }
        }

        if (current) {
            return false;
        }

        current = getnew();
        return !current;
    }

    void run()
    {
        while (running) {
            bool canrest = tick();
            if (canrest) {
                std::unique_lock<std::mutex> lk(mutex);
                cv.wait(lk, [this]{ return ready; });
                ready = false;
            }
        }
    }


public:

    SleepyLoadingThread(std::function<std::shared_ptr<T>()> getnew)
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
        if (thread.joinable()) {
            thread.join();
        }
    }

    void detach() {
        if (thread.joinable()) {
            thread.detach();
        }
    }

    void notify() {
        {
            std::lock_guard<std::mutex> lk(mutex);
            ready = true;
        }
        cv.notify_one();
    }

    std::shared_ptr<T> getCurrent() {
        return current;
    }

};

