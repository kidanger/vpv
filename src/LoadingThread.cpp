#include "Progressable.hpp"
#include "events.hpp"
#include "globals.hpp"

#include "LoadingThread.hpp"

bool LoadingThread::tick()
{
    // load the queue
    if (!queue.empty()) {
        // NOTE: front(), not back(): pop() removes the front, so taking back()
        // meant we advanced one item but retired another as soon as the queue
        // held more than one element.
        std::shared_ptr<Progressable> p = queue.front();
        p->progress();
        // if the provider is used somewhere else, refresh the screen
        // 2 because queue + local variable p
        if (p.use_count() != 2) {
            gActive = std::max(gActive, 2);
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
