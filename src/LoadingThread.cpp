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
        if (seq->imageprovider && !seq->imageprovider->isLoaded()) {
            seq->imageprovider->progress();
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

