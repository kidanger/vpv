#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdlib>

#include "Image.hpp"
#include "ImageCache.hpp"
#include "globals.hpp"

static std::unordered_map<std::string, std::shared_ptr<Image>> cache;
static std::mutex lock;
static size_t cacheSize = 0;

bool ImageCache::has(const std::string& key)
{
    bool has = false;
    lock.lock();
    if (cache.find(key) != cache.end())
        has = true;
    lock.unlock();
    return has;
}

std::shared_ptr<Image> ImageCache::get(const std::string& key)
{
    lock.lock();
    std::shared_ptr<Image> image = cache[key];
    lock.unlock();
    return image;
}

void ImageCache::store(const std::string& key, std::shared_ptr<Image> image)
{
    lock.lock();
    cache[key] = image;
    image->is_cached = true;
    lock.unlock();
}

void ImageCache::flush()
{
    lock.lock();
    cache.clear();
    cacheSize = 0;
    lock.unlock();
}

