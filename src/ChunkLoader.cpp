#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <vector>

#include "ChunkLoader.hpp"
#include "Image.hpp"
#include "globals.hpp"

std::shared_ptr<Chunk> LazyChunkSource::fetch(size_t level, BandIndex band, size_t cx, size_t cy)
{
    ChunkKey key { level, band, cx, cy };

    {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = ready.find(key);
        if (it != ready.end()) {
            // handed over to the caller (Image retains it from now on)
            std::shared_ptr<Chunk> chunk = it->second;
            ready.erase(it);
            return chunk;
        }

        if (failed.count(key))
            return nullptr;

        if (!queued.count(key)) {
            queued.insert(key);
            pending.push_back(key);
            while (pending.size() > MAX_PENDING) {
                queued.erase(pending.back());
                pending.pop_back();
            }
        }
    }

    ChunkLoader::notify();
    return nullptr;
}

std::shared_ptr<Chunk> LazyChunkSource::fetchBlocking(size_t level, BandIndex band, size_t cx, size_t cy)
{
    ChunkKey key { level, band, cx, cy };
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = ready.find(key);
        if (it != ready.end())
            return it->second;
        if (failed.count(key))
            return nullptr;
    }

    std::shared_ptr<Chunk> chunk = read(level, band, cx, cy);
    if (!chunk) {
        std::lock_guard<std::mutex> lock(mutex);
        failed.insert(key);
    }
    return chunk;
}

bool LazyChunkSource::hasPending() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return !pending.empty();
}

bool LazyChunkSource::loadOne()
{
    ChunkKey key;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (pending.empty())
            return false;
        key = pending.front();
        pending.pop_front();
    }

    // read() is blocking and must not hold our mutex: fetch() has to stay
    // responsive while the disk is busy
    std::shared_ptr<Chunk> chunk = read(key.level, key.band, key.cx, key.cy);

    {
        std::lock_guard<std::mutex> lock(mutex);
        queued.erase(key);
        if (chunk) {
            ready[key] = chunk;
        } else {
            failed.insert(key);
        }
    }
    return true;
}

namespace ChunkLoader {

static std::mutex mutex;
static std::condition_variable cv;
static bool ready = false;
static bool running = false;
// Never destroyed on purpose: the app is allowed to exit brutally while a read
// is stuck in GDAL, and a static std::thread that is still joinable at exit
// would call std::terminate.
static std::thread* thread = nullptr;
static std::vector<std::weak_ptr<LazyChunkSource>> sources;

struct StatsJob {
    std::weak_ptr<Image> image;
    std::array<size_t, 3> bands;
};
static std::deque<StatsJob> statsJobs;

void add(const std::shared_ptr<LazyChunkSource>& source)
{
    std::lock_guard<std::mutex> lock(mutex);
    sources.erase(std::remove_if(sources.begin(), sources.end(),
                      [](const std::weak_ptr<LazyChunkSource>& w) { return w.expired(); }),
        sources.end());
    sources.push_back(source);
}

void requestStats(const std::shared_ptr<Image>& image, std::array<size_t, 3> bands)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        statsJobs.push_back({ image, bands });
    }
    notify();
}

static bool tickStats()
{
    StatsJob job;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (statsJobs.empty())
            return false;
        job = statsJobs.front();
        statsJobs.pop_front();
    }
    std::shared_ptr<Image> image = job.image.lock();
    if (!image)
        return true; // the image is gone; that still counts as progress
    image->computeStats(job.bands);
    // the colormap is waiting on this to initialise itself
    gActive = std::max(gActive, 2);
    return true;
}

static bool tick()
{
    // Statistics first, on purpose: see requestStats().
    if (tickStats())
        return true;

    std::vector<std::shared_ptr<LazyChunkSource>> alive;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& w : sources) {
            if (std::shared_ptr<LazyChunkSource> s = w.lock()) {
                alive.push_back(s);
            }
        }
    }

    bool didsomething = false;
    for (const auto& s : alive) {
        if (s->loadOne()) {
            didsomething = true;
            // a chunk landed: the display has to be asked to draw again,
            // otherwise it will never come and claim it
            gActive = std::max(gActive, 2);
        }
    }
    return didsomething;
}

static void run()
{
    while (running) {
        if (!tick()) {
            std::unique_lock<std::mutex> lk(mutex);
            cv.wait_for(lk, std::chrono::milliseconds(100), [] { return ready || !running; });
            ready = false;
        }
    }
}

void start()
{
    running = true;
    thread = new std::thread(run);
}

void stop()
{
    running = false;
    notify();
}

void join()
{
    if (thread && thread->joinable()) {
        thread->join();
    }
}

void notify()
{
    {
        std::lock_guard<std::mutex> lk(mutex);
        ready = true;
    }
    cv.notify_one();
}

}
