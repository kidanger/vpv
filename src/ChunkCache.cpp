#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#ifdef USE_GDAL
#include <gdal.h>
#endif

#include "Chunk.hpp"
#include "ChunkCache.hpp"
#include "globals.hpp"

namespace ChunkCache {

namespace {
    struct Entry {
        std::shared_ptr<Chunk> chunk;
        const Image* owner;
        uint64_t used; // LRU clock
        uint64_t frame; // frame it was last used in
    };

    std::mutex mutex;
    std::unordered_map<const Chunk*, Entry> entries;
    uint64_t clock = 0;
    uint64_t frame = 0;
    size_t chunkBytes = 0;
    size_t imageBytes = 0;

    size_t limit()
    {
        return gCacheLimitMB * 1000000;
    }

    // Called with 'mutex' held.
    void evict()
    {
        size_t max = limit();
        if (!max)
            return; // not configured yet: do not evict everything

        while (chunkBytes + imageBytes > max) {
            // O(n) over resident chunks, once per read. n is in the hundreds and
            // a read is orders of magnitude slower, so a heap would buy nothing.
            auto oldest = entries.end();
            for (auto it = entries.begin(); it != entries.end(); it++) {
                if (it->second.frame == frame)
                    continue; // in use this frame
                if (oldest == entries.end() || it->second.used < oldest->second.used)
                    oldest = it;
            }

            if (oldest == entries.end())
                break; // everything left is needed right now; go over budget
            chunkBytes -= oldest->second.chunk->bytes();
            entries.erase(oldest);
        }
    }
}

void retain(const std::shared_ptr<Chunk>& chunk, const Image* owner)
{
    if (!chunk)
        return;

    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(chunk.get());
    if (it != entries.end()) {
        it->second.used = ++clock;
        it->second.frame = frame;
        if (owner)
            it->second.owner = owner;
        return;
    }

    entries[chunk.get()] = Entry { chunk, owner, ++clock, frame };
    chunkBytes += chunk->bytes();
    evict();
}

void forget(const Image* owner)
{
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->second.owner == owner) {
            chunkBytes -= it->second.chunk->bytes();
            it = entries.erase(it);
        } else {
            it++;
        }
    }
}

void beginFrame()
{
    std::lock_guard<std::mutex> lock(mutex);
    frame++;
}

size_t bytes()
{
    std::lock_guard<std::mutex> lock(mutex);
    return chunkBytes;
}

void setImageBytes(size_t bytes)
{
    std::lock_guard<std::mutex> lock(mutex);
    imageBytes = bytes;
    evict();
}

size_t totalBytes()
{
    std::lock_guard<std::mutex> lock(mutex);
    return chunkBytes + imageBytes;
}

void flush()
{
    std::lock_guard<std::mutex> lock(mutex);
    entries.clear();
    chunkBytes = 0;
}

size_t processBytes()
{
#ifdef __linux__
    // statm's second field is the resident set, in pages
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f)
        return 0;
    unsigned long total = 0, resident = 0;
    int n = fscanf(f, "%lu %lu", &total, &resident);
    fclose(f);
    if (n != 2)
        return 0;
    return (size_t)resident * 4096;
#else
    return 0;
#endif
}

size_t gdalCacheBytes()
{
#ifdef USE_GDAL
    return (size_t)GDALGetCacheUsed64();
#else
    return 0;
#endif
}

}

#include <doctest.h>

TEST_CASE("ChunkCache evicts the least recently used chunk")
{
    ChunkCache::flush();
    ChunkCache::setImageBytes(0);
    size_t saved = gCacheLimitMB;

    auto make = [](float v) {
        auto c = std::make_shared<Chunk>(512, 512); // 1MB
        c->pixels[0] = v;
        return c;
    };
    std::shared_ptr<Chunk> a = make(1), b = make(2), c = make(3);
    std::weak_ptr<Chunk> wa = a, wb = b, wc = c;
    REQUIRE(a->bytes() == 1024 * 1024);

    gCacheLimitMB = 2; // room for two chunks
    ChunkCache::beginFrame();
    ChunkCache::retain(a, nullptr);
    ChunkCache::retain(b, nullptr);
    CHECK(ChunkCache::bytes() == 2 * a->bytes());

    // a and b were used in this frame, so the third one has to go over budget
    ChunkCache::retain(c, nullptr);
    CHECK(ChunkCache::bytes() == 3 * a->bytes());

    // next frame: nothing is protected any more, so the oldest goes
    ChunkCache::beginFrame();
    ChunkCache::retain(b, nullptr); // b becomes the most recent
    a.reset();
    b.reset();
    c.reset();
    // the cache is the only owner now
    CHECK(bool(wb.lock()));
    ChunkCache::retain(make(4), nullptr);
    CHECK(!wa.lock()); // evicted: oldest
    CHECK(bool(wb.lock())); // kept: touched again
    CHECK(ChunkCache::bytes() <= 2 * 1024 * 1024);

    ChunkCache::flush();
    gCacheLimitMB = saved;
}

TEST_CASE("ChunkCache shares its budget with the image cache")
{
    ChunkCache::flush();
    size_t saved = gCacheLimitMB;
    gCacheLimitMB = 2;

    auto chunk = std::make_shared<Chunk>(512, 512); // 1MB
    std::weak_ptr<Chunk> weak = chunk;
    ChunkCache::beginFrame();
    ChunkCache::retain(chunk, nullptr);
    CHECK(ChunkCache::totalBytes() == 1024 * 1024);

    // whole images filling the budget push chunks out, and vice versa
    ChunkCache::beginFrame();
    chunk.reset();
    ChunkCache::setImageBytes(2 * 1024 * 1024);
    CHECK(!weak.lock());
    CHECK(ChunkCache::bytes() == 0);
    CHECK(ChunkCache::totalBytes() == 2 * 1024 * 1024);

    ChunkCache::setImageBytes(0);
    ChunkCache::flush();
    gCacheLimitMB = saved;
}
