#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Image.hpp"
#include "ImageCache.hpp"
#include "events.hpp"
#include "globals.hpp"

namespace ImageCache {
static std::unordered_map<std::string, std::shared_ptr<Image>> cache;
static std::mutex lock;
static size_t cacheSize = 0;
static bool cacheFull = false;

bool has(const std::string& key)
{
    std::lock_guard<std::mutex> _lock(lock);
    return cache.find(key) != cache.end();
}

std::shared_ptr<Image> get(const std::string& key)
{
    std::lock_guard<std::mutex> _lock(lock);
    std::shared_ptr<Image> image = cache[key];
    return image;
}

std::shared_ptr<Image> getById(const std::string& id)
{
    std::lock_guard<std::mutex> _lock(lock);
    for (const auto& c : cache) {
        if (c.second->ID == id) {
            return c.second;
        }
    }
    return nullptr;
}

static bool hasSpaceFor(const Image& image)
{
    size_t need = image.w * image.h * image.c * sizeof(float);
    size_t limit = gCacheLimitMB * 1000000;
    return cacheSize + need < limit;
}

static bool makeRoomFor(const Image& image)
{
    size_t need = image.w * image.h * image.c * sizeof(float);
    size_t limit = gCacheLimitMB * 1000000;

    if (need > limit)
        return false;
    while (cacheSize + need > limit) {
        std::string worst;

        // FIXME: slow, use a priority queue to sort old images upto a given space limit
        double last = -1;
        for (auto& it : cache) {
            std::shared_ptr<Image> img = it.second;
            uint64_t t = img->lastUsed;
            double imgtime = letTimeFlow(&t);
            if (imgtime > last) {
                worst = it.first;
                last = imgtime;
            }
        }

        remove_rec(worst);
    }
    return true;
}

void store(const std::string& key, std::shared_ptr<Image> image)
{
    std::lock_guard<std::mutex> _lock(lock);

    letTimeFlow(&image->lastUsed);

    // check whether we already have it
    auto i = cache.find(key);
    if (i != cache.end()) {
        //puts(0);
        exit(1);
        return;
    }
    if (!hasSpaceFor(*image)) {
        cacheFull = true;
        if (!makeRoomFor(*image)) {
            return;
        }
    } else {
        cacheFull = false;
    }
    cache[key] = image;
    cacheSize += image->w * image->h * image->c * sizeof(float);
}

bool remove_rec(const std::string& key)
{
    auto i = cache.find(key);
    if (i != cache.end()) {
        std::shared_ptr<Image> image = i->second;
        cache.erase(i);
        cacheSize -= image->w * image->h * image->c * sizeof(float);
        for (const auto& k : image->usedBy) {
            remove_rec(k);
        }
        return true;
    }
    return false;
}

bool remove(const std::string& key)
{
    std::lock_guard<std::mutex> _lock(lock);
    return remove_rec(key);
}

bool isFull()
{
    return cacheFull;
}

void flush()
{
    std::lock_guard<std::mutex> _lock(lock);
    cache.clear();
    cacheSize = 0;
    cacheFull = false;
}

namespace Error {
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex lock;

    bool has(const std::string& key)
    {
        std::lock_guard<std::mutex> _lock(lock);
        bool has = false;
        if (cache.find(key) != cache.end())
            has = true;
        return has;
    }

    std::string get(const std::string& key)
    {
        std::lock_guard<std::mutex> _lock(lock);
        const std::string message = cache[key];
        return message;
    }

    void store(const std::string& key, const std::string& message)
    {
        std::lock_guard<std::mutex> _lock(lock);
        cache[key] = message;
    }

    bool remove(const std::string& key)
    {
        std::lock_guard<std::mutex> _lock(lock);
        auto i = cache.find(key);
        if (i != cache.end()) {
            cache.erase(i);
            return true;
        }
        return false;
    }

    void flush()
    {
        std::lock_guard<std::mutex> _lock(lock);
        cache.clear();
    }
}
}
