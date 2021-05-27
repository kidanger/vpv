#include "ImageRegistry.hpp"

static ImageRegistry gImageRegistry;


ImageRegistry::Status ImageRegistry::getStatus(Key key)
{
    return UNKNOWN;
}

float ImageRegistry::getProgress(Key key)
{
    return 0.f;
}

std::shared_ptr<Image> ImageRegistry::getImage(Key key)
{
    return nullptr;
}

std::string ImageRegistry::getError(Key key)
{
    return "";
}

ImageRegistry& getGlobalImageRegistry()
{
    return gImageRegistry;
}

