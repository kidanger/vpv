#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <vector>

#include "ChunkCache.hpp"
#include "ChunkLoader.hpp"
#include "Image.hpp"
#include "globals.hpp"

std::shared_ptr<Chunk> LazyChunkSource::fetch(size_t level, BandIndex band, size_t cx, size_t cy)
{
    ChunkKey key { level, band, cx, cy };
    // stamped with the frame the request was made in, so that a request that
    // stops coming back can be dropped (dropStaleRequests)
    uint64_t frame = ChunkCache::currentFrame();

    {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = ready.find(key);
        if (it != ready.end()) {
            // handed over to the caller (Image retains it from now on); an entry
            // whose chunk the cache has already evicted is just stale
            std::shared_ptr<Chunk> chunk = it->second.lock();
            ready.erase(it);
            if (chunk)
                return chunk;
        }

        if (failed.count(key))
            return nullptr;

        auto q = queued.find(key);
        if (q != queued.end()) {
            q->second = frame; // still wanted
        } else {
            queued.emplace(key, frame);
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
        if (it != ready.end()) {
            if (std::shared_ptr<Chunk> chunk = it->second.lock())
                return chunk;
            ready.erase(it);
        }
        if (failed.count(key))
            return nullptr;
        inflight++;
    }

    std::string error;
    std::shared_ptr<Chunk> chunk = read(level, band, cx, cy, error);
    {
        std::lock_guard<std::mutex> lock(mutex);
        inflight--;
    }
    if (!chunk) {
        recordFailure(key, error);
    }
    return chunk;
}

void LazyChunkSource::recordFailure(const ChunkKey& key, const std::string& error)
{
    std::lock_guard<std::mutex> lock(mutex);
    failed.insert(key);
    if (!error.empty() && errors.size() < MAX_ERRORS
        && std::find(errors.begin(), errors.end(), error) == errors.end()) {
        errors.push_back(error);
    }
}

bool LazyChunkSource::hasPending() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return !pending.empty() || inflight > 0;
}

ChunkSource::Status LazyChunkSource::status() const
{
    std::lock_guard<std::mutex> lock(mutex);
    Status s;
    s.pending = pending.size() + inflight;
    s.failed = failed.size();
    s.errors = errors;
    return s;
}

bool LazyChunkSource::isPending(size_t level, BandIndex band, size_t cx, size_t cy) const
{
    // "Not failed" is the honest answer, rather than "queued or being read":
    // the display asks for every visible chunk on every frame, so a chunk that
    // was dropped from the tail of the queue is still going to be read. Only a
    // read that failed for good is never coming.
    std::lock_guard<std::mutex> lock(mutex);
    return !failed.count(ChunkKey { level, band, cx, cy });
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
        // still counts as pending for the indicator: the read has not finished
        inflight++;
    }

    // read() is blocking and must not hold our mutex: fetch() has to stay
    // responsive while the disk is busy
    std::string error;
    std::shared_ptr<Chunk> chunk = read(key.level, key.band, key.cx, key.cy, error);

    // The cache owns it from here, so it counts against the RAM budget even
    // before anyone claims it, and may be evicted if nobody ever does. 'owner'
    // is not known yet: the Image that claims it adopts it then.
    ChunkCache::retain(chunk, nullptr);

    {
        std::lock_guard<std::mutex> lock(mutex);
        queued.erase(key);
        inflight--;
        if (chunk) {
            ready[key] = chunk;
        }
    }
    if (!chunk) {
        recordFailure(key, error);
    }
    return true;
}

size_t LazyChunkSource::dropStaleRequests(uint64_t frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    size_t dropped = 0;
    auto stale = [&](const ChunkKey& key) {
        auto q = queued.find(key);
        if (q == queued.end())
            return true; // not wanted at all any more (should not happen)
        if (frame - q->second < STALE_AFTER_FRAMES)
            return false;
        queued.erase(q);
        dropped++;
        return true;
    };
    pending.erase(std::remove_if(pending.begin(), pending.end(), stale), pending.end());
    return dropped;
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

static std::vector<std::shared_ptr<LazyChunkSource>> aliveSources()
{
    std::vector<std::shared_ptr<LazyChunkSource>> alive;
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& w : sources) {
        if (std::shared_ptr<LazyChunkSource> s = w.lock()) {
            alive.push_back(s);
        }
    }
    return alive;
}

void beginFrame()
{
    // ChunkCache::beginFrame has just moved to the new frame, so a request last
    // asked for during the frame before the previous one is one nobody wants.
    uint64_t frame = ChunkCache::currentFrame();
    for (const auto& s : aliveSources()) {
        s->dropStaleRequests(frame);
    }
}

static bool tick()
{
    // Statistics first, on purpose: see requestStats().
    if (tickStats())
        return true;

    bool didsomething = false;
    for (const auto& s : aliveSources()) {
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

#include <doctest.h>

namespace {

// Counts reads; every chunk read succeeds.
class CountingLazySource : public LazyChunkSource {
public:
    size_t reads = 0;

    std::vector<Level> describeLevels() const override
    {
        return { Level(4 * CHUNK_SIZE, CHUNK_SIZE, 1.0, 1.0) };
    }

protected:
    std::shared_ptr<Chunk> read(size_t, BandIndex, size_t, size_t, std::string&) override
    {
        reads++;
        return std::make_shared<Chunk>(CHUNK_SIZE, CHUNK_SIZE);
    }
};

}

TEST_CASE("a queued read that stops being asked for is cancelled")
{
    auto src = std::make_shared<CountingLazySource>();

    // frame F: two chunks are visible, neither is resident
    ChunkCache::beginFrame();
    CHECK(!src->fetch(0, 0, 0, 0));
    CHECK(!src->fetch(0, 0, 1, 0));
    CHECK(src->status().pending == 2);

    // frame F+1: the view has moved, only the first one is still asked for. One
    // frame of grace, so nothing is dropped yet.
    ChunkCache::beginFrame();
    src->dropStaleRequests(ChunkCache::currentFrame());
    CHECK(src->status().pending == 2);
    CHECK(!src->fetch(0, 0, 0, 0));

    // frame F+2: the second one has not been asked for in a whole frame
    ChunkCache::beginFrame();
    src->dropStaleRequests(ChunkCache::currentFrame());
    CHECK(src->status().pending == 1);

    // and what is left is the chunk that is still wanted
    CHECK(src->loadOne());
    CHECK(src->reads == 1);
    CHECK(src->status().pending == 0);
    CHECK(bool(src->fetch(0, 0, 0, 0)));

    SUBCASE("a cancelled read is queued again if it comes back into view")
    {
        ChunkCache::beginFrame();
        CHECK(!src->fetch(0, 0, 1, 0));
        CHECK(src->status().pending == 1);
        CHECK(src->loadOne());
        CHECK(src->reads == 2);
    }
}

TEST_CASE("cancelling never drops a read that is being served")
{
    auto src = std::make_shared<CountingLazySource>();

    ChunkCache::beginFrame();
    CHECK(!src->fetch(0, 0, 0, 0));
    CHECK(src->loadOne()); // popped from 'pending', read, and now in 'ready'

    // nobody asks again for a long time: there is nothing left to cancel, and
    // the chunk that was read stays claimable (until the cache evicts it)
    for (int i = 0; i < 4; i++) {
        ChunkCache::beginFrame();
        src->dropStaleRequests(ChunkCache::currentFrame());
    }
    CHECK(src->status().pending == 0);
    CHECK(bool(src->fetch(0, 0, 0, 0)));
    CHECK(src->reads == 1);
}
