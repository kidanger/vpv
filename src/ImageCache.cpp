#include <cstdlib>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ChunkCache.hpp"
#include "Image.hpp"
#include "ImageCache.hpp"
#include "events.hpp"
#include "globals.hpp"

namespace ImageCache {
static std::unordered_map<std::string, std::shared_ptr<Image>> cache;
static std::mutex lock;
static size_t cacheSize = 0;
static bool cacheFull = false;
// what each entry contributed to cacheSize when it was stored
static std::unordered_map<std::string, size_t> storedSize;
// eviction order, most recently stored first. lastUsed is only ever bumped when
// an image is stored, so this is exactly the order the old O(n) scan computed.
static std::list<std::string> lru;

bool has(const std::string& key)
{
    std::lock_guard<std::mutex> _lock(lock);
    return cache.find(key) != cache.end();
}

std::shared_ptr<Image> get(const std::string& key)
{
    std::lock_guard<std::mutex> _lock(lock);
    std::shared_ptr<Image> image = cache[key];
    if (image)
        image->frame = ChunkCache::currentFrame();
    return image;
}

std::shared_ptr<Image> getById(const std::string& id)
{
    std::lock_guard<std::mutex> _lock(lock);
    for (const auto& c : cache) {
        if (c.second->ID == id) {
            c.second->frame = ChunkCache::currentFrame();
            return c.second;
        }
    }
    return nullptr;
}

// The budget is shared with the chunk cache: both hold pixels, and a big image
// being panned has to be able to push cached frames out and vice versa. The
// running total therefore lives in ChunkCache, which is the one that can evict
// in response.
static bool hasSpaceFor(const Image& image)
{
    size_t need = image.memoryFootprint();
    size_t limit = gCacheLimitMB * 1000000;
    return ChunkCache::totalBytes() + need < limit;
}

static bool makeRoomFor(const Image& image)
{
    size_t need = image.memoryFootprint();
    size_t limit = gCacheLimitMB * 1000000;

    if (need > limit)
        return false;
    while (ChunkCache::totalBytes() + need > limit) {
        if (lru.empty()) {
            // resident chunks alone are over the limit; they are the ones being
            // looked at, so this image waits rather than evicting them
            return false;
        }
        remove_rec(lru.back());
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
    // A lazy image reports nothing: its chunks are the chunk cache's, accounted
    // for there against the same limit.
    cacheSize += image->memoryFootprint();
    storedSize[key] = image->memoryFootprint();
    lru.push_front(key);
    ChunkCache::setImageBytes(cacheSize);
}

bool remove_rec(const std::string& key)
{
    auto i = cache.find(key);
    if (i != cache.end()) {
        std::shared_ptr<Image> image = i->second;
        cache.erase(i);
        // subtract exactly what was added, whatever the image holds now
        cacheSize -= storedSize[key];
        storedSize.erase(key);
        lru.remove(key);
        ChunkCache::setImageBytes(cacheSize);
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
    // an image handed out during the current frame is being displayed: dropping
    // it would only have it decoded again before anything else got drawn
    uint64_t frame = ChunkCache::currentFrame();
    std::vector<std::string> doomed;
    for (const auto& c : cache) {
        // handed out during the current frame and still held by somebody: it is
        // what is being drawn, and dropping it would only have it decoded again
        // before anything reached the screen. An image nobody holds any more
        // goes even if it was displayed earlier in this frame, which is what
        // makes "forget the image, then flush" reload it.
        if (c.second && c.second->frame == frame && c.second.use_count() > 1)
            continue;
        doomed.push_back(c.first);
    }
    for (const std::string& key : doomed)
        remove_rec(key); // may take entries that depend on it along
    cacheFull = false;
    // the images are gone, but the cache is what owns their chunks
    ChunkCache::flush();
    ChunkCache::setImageBytes(cacheSize);
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
