#include "ImageRegistry.hpp"

static ImageRegistry gImageRegistry;

float ImageRegistry::getProgress(Key key)
{
    return 0.f;
}

ImageRegistry& getGlobalImageRegistry()
{
    return gImageRegistry;
}

moodycamel::BlockingConcurrentQueue<std::pair<std::shared_ptr<ImageProvider>, ImageRegistry::Key>> Q;
