#pragma once

#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>

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

    Status getStatus(Key key);
    float getProgress(Key key);
    std::shared_ptr<Image> getImage(Key key);
    std::string getError(Key key);

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

    void putImage(Key key, std::shared_ptr<Image> image)
    {
        images[key] = image;
        assert(statuses[key] == LOADING);
        statuses[key] = LOADED;
    }

private:
    std::unordered_map<Key, Status> statuses;
    std::unordered_map<Key, std::shared_ptr<Image>> images;
};

ImageRegistry& getGlobalImageRegistry();
