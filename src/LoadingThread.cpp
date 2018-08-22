#include "LoadingThread.hpp"

#include "events.hpp"
#include "globals.hpp"
#include "Sequence.hpp"
#include "Image.hpp"
#include "ImageProvider.hpp"
#include "ImageCollection.hpp"
#include "Player.hpp"


bool LoadingThread::tick()
{
    // load the queue
    if (!queue.empty()) {
        std::shared_ptr<ImageProvider> p = queue.back();
        p->progress();
        if (p->isLoaded()) {
            queue.pop();
            gActive = std::max(gActive, 2);
        }
    }

    if (!queue.empty()) {
        return false;
    }

    // fill the queue with images to be displayed
    for (auto seq : gSequences) {
        std::shared_ptr<ImageProvider> provider = seq->imageprovider;
        if (provider && !provider->isLoaded()) {
            queue.push(provider);
            return false;
        }
    }

    // fill the queue with futur frames
    for (int i = 1; i < 100; i++) {
        for (auto seq : gSequences) {
            ImageCollection* collection = seq->collection;
            int frame = (seq->player->frame + i - 1) % collection->getLength();
            std::shared_ptr<ImageProvider> provider = collection->getImageProvider(frame);
            if (!provider->isLoaded()) {
                queue.push(provider);
                return false;
            }
        }
    }

    return true;
}

void LoadingThread::run()
{
    while (running) {
        bool canrest = tick();
        if (canrest) {
            stopTime(5);
        }
    }
}

