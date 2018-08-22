#include "LoadingThread.hpp"

#include "events.hpp"
#include "globals.hpp"
#include "Sequence.hpp"
#include "Image.hpp"
#include "ImageProvider.hpp"
#include "Player.hpp"


bool LoadingThread::tick()
{
    bool canrest = true;
    for (auto seq : gSequences) {
        ImageProvider* provider = seq->imageprovider;
        if (provider && !provider->isLoaded()) {
            provider->progress();
            if (provider->isLoaded()) {
                gActive = std::max(gActive, 2);
            }
            canrest = false;
        }
    }
    return canrest;
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

