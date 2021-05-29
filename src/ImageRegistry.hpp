#pragma once

#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>

#include "ImageProvider.hpp"

struct Image;

class ImageRegistry {
public:
    enum Status {
        UNKNOWN,
        //PLANNED,
        LOADING,
        LOADED,
    };

    typedef std::string Key;

    Status getStatus(Key key)
    {
        auto it = statuses.find(key);
        if (it != statuses.end()) {
            return it->second;
        }
        return UNKNOWN;
    }

    float getProgress(Key key);

    ImageProvider::Result getImage(Key key)
    {
        auto it = images.find(key);
        if (it != images.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool getOrSetLoading(Key key)
    {
        auto it = statuses.find(key);
        if (it != statuses.end()) {
            if (it->second == LOADING) {
                return false;
            }
        }
        statuses[key] = LOADING;
        return true;
    }

    void putImage(Key key, ImageProvider::Result image)
    {
        images[key] = image;
        statuses[key] = LOADED;
    }

private:
    std::unordered_map<Key, Status> statuses;
    std::unordered_map<Key, ImageProvider::Result> images;
};

ImageRegistry& getGlobalImageRegistry();

#include "blockingconcurrentqueue.h"
moodycamel::ProducerToken getToken();
void flushQueue(const moodycamel::ProducerToken& token);
std::pair<std::shared_ptr<ImageProvider>, ImageRegistry::Key> popQueue();
void pushQueue(const moodycamel::ProducerToken& token, std::shared_ptr<const ImageCollection> collection, int index);
void flushAndPushQueue(const moodycamel::ProducerToken& token, std::shared_ptr<const ImageCollection> collection, int index);

