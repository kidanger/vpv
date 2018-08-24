#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdlib>

#include "Image.hpp"
#include "ImageCache.hpp"
#include "globals.hpp"
#include "events.hpp"

namespace ImageCache {
    static std::unordered_map<std::string, std::shared_ptr<Image>> cache;
    static std::mutex lock;
    static size_t cacheSize = 0;
    static bool cacheFull = false;

    bool has(const std::string& key)
    {
        std::lock_guard<std::mutex> _lock(lock);
        bool has = false;
        if (cache.find(key) != cache.end())
            has = true;
        return has;
    }

    std::shared_ptr<Image> get(const std::string& key)
    {
        std::lock_guard<std::mutex> _lock(lock);
        std::shared_ptr<Image> image = cache[key];
        return image;
    }

    static bool hasSpaceFor(const std::shared_ptr<Image>& image)
    {
        size_t need = image->w * image->h * image->format * sizeof(float);
        size_t limit = gCacheLimitMB*1000000;
        return cacheSize + need < limit;
    }

    static bool makeRoomFor(const std::shared_ptr<Image>& image)
    {
        size_t need = image->w * image->h * image->format * sizeof(float);
        size_t limit = gCacheLimitMB*1000000;

        if (need > limit) return false;
        while (cacheSize + need > limit) {
            std::string worst;
            std::shared_ptr<Image> worstImg;

            // FIXME: slow, use a priority queue to sort old images upto a given space limit
            double last = -1;
            for (auto it = cache.begin(); it != cache.end(); it++) {
                std::shared_ptr<Image> img = it->second;
                uint64_t t = img->lastUsed;
                double imgtime = letTimeFlow(&t);
                if (imgtime > last) {
                    worst = it->first;
                    worstImg = img;
                    last = imgtime;
                }
            }

            cache.erase(worst);
            cacheSize -= worstImg->w * worstImg->h * worstImg->format * sizeof(float);
            printf("remove %s\n", worst.c_str());
        }
        return true;
    }

    void store(const std::string& key, std::shared_ptr<Image> image)
    {
        std::lock_guard<std::mutex> _lock(lock);

        letTimeFlow(&image->lastUsed);

        // check weither we already have it
        auto i = cache.find(key);
        if (i != cache.end()) {
            return;
        }
        if (!hasSpaceFor(image)) {
            cacheFull = true;
            if (!makeRoomFor(image)) {
                return;
            }
        } else {
            cacheFull = false;
        }
        cache[key] = image;
        cacheSize += image->w * image->h * image->format * sizeof(float);
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
            bool has = false;
            lock.lock();
            if (cache.find(key) != cache.end())
                has = true;
            lock.unlock();
            return has;
        }

        std::string get(const std::string& key)
        {
            lock.lock();
            const std::string message = cache[key];
            lock.unlock();
            return message;
        }

        void store(const std::string& key, const std::string& message)
        {
            lock.lock();
            cache[key] = message;
            lock.unlock();
        }
    }
}

