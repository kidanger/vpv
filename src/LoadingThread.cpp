#include "Progressable.hpp"
#include "events.hpp"
#include "globals.hpp"

#include "LoadingThread.hpp"

bool LoadingThread::tick()
{
    // load the queue
    if (!queue.empty()) {
        std::shared_ptr<Progressable> p = queue.back();
        p->progress();
        // if the provider is used somewhere else, refresh the screen
        // 2 because queue + local variable p
        if (p.use_count() != 2) {
            gActive |= 2;
        }
        if (p->isLoaded()) {
            queue.pop();
        }
    }

    if (!queue.empty()) {
        return false;
    }

    std::shared_ptr<Progressable> p = getnew();
    if (p) {
        queue.push(p);
        return false;
    }

    return true;
}

void LoadingThread::run()
{
    while (running) {
        bool canrest = tick();
        if (canrest) {
            stopTime(10);
        }
    }
}
