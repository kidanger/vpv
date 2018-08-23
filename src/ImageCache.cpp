#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdlib>

#include "Image.hpp"
#include "ImageCache.hpp"
#include "globals.hpp"

namespace ImageCache {
    static std::unordered_map<std::string, std::shared_ptr<Image>> cache;
    static std::mutex lock;
    static size_t cacheSize = 0;

    bool has(const std::string& key)
    {
        bool has = false;
        lock.lock();
        if (cache.find(key) != cache.end())
            has = true;
        lock.unlock();
        return has;
    }

    std::shared_ptr<Image> get(const std::string& key)
    {
        lock.lock();
        std::shared_ptr<Image> image = cache[key];
        lock.unlock();
        return image;
    }

    void store(const std::string& key, std::shared_ptr<Image> image)
    {
        lock.lock();
        cache[key] = image;
        lock.unlock();
    }


    void flush()
    {
        lock.lock();
        cache.clear();
        cacheSize = 0;
        lock.unlock();
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

