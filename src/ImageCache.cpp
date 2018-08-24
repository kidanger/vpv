#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <cstdlib>

#include "Image.hpp"
#include "ImageCache.hpp"
#include "globals.hpp"
#include "events.hpp"

#include "ImageProvider.hpp"

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

            // FIXME: slow, use a priority queue to sort old images upto a given space limit
            double last = -1;
            for (auto it = cache.begin(); it != cache.end(); it++) {
                std::shared_ptr<Image> img = it->second;
                uint64_t t = img->lastUsed;
                double imgtime = letTimeFlow(&t);
                if (imgtime > last) {
                    worst = it->first;
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
            LOG2("store image " << key << " but we already have it...");
            puts(0); exit(1);
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
        LOG2("store image " << key << " " << image);
    }

    bool remove_rec(const std::string& key)
    {
        auto i = cache.find(key);
        if (i != cache.end()) {
            std::shared_ptr<Image> image = i->second;
            LOG2("remove image " << key << " " << image);
            cache.erase(i);
            cacheSize -= image->w * image->h * image->format * sizeof(float);
            for (auto k : image->usedBy) {
                LOG2("try remove " << k);
                remove_rec(k);
            }
            return true;
        }
        return false;
    }

    bool remove(const std::string& key)
    {
        std::lock_guard<std::mutex> _lock(lock);
        LOG2("ask remove image " << key);
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
            LOG2("store error " << key << " " << message);
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
    }
}

