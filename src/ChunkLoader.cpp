#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
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

        // already being read: not interruptible, and re-queueing it would read it
        // twice
        if (loading.count(key))
            return nullptr;

        auto q = queued.find(key);
        if (q != queued.end()) {
            // Still wanted, and wanted *now*: re-prioritise it rather than just
            // refreshing its stamp. Without this the queue would keep serving
            // things in first-request order, which one frame after a pan means
            // serving the chunks the view has just left before the ones under
            // the cursor.
            RequestPriority prio { frame, nextSeq++ };
            pending.erase({ q->second, key });
            pending.insert({ prio, key });
            q->second = prio;
        } else {
            RequestPriority prio { frame, nextSeq++ };
            queued.emplace(key, prio);
            pending.insert({ prio, key });
            while (pending.size() > MAX_PENDING) {
                // the least wanted thing of the oldest frame still queued, which
                // may well be the request just made if a single frame asks for
                // more than MAX_PENDING chunks: it is then the one furthest from
                // the centre, so dropping it is right
                auto worst = std::prev(pending.end());
                queued.erase(worst->second);
                pending.erase(worst);
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
    recordFailureLocked(key, error);
}

void LazyChunkSource::recordFailureLocked(const ChunkKey& key, const std::string& error)
{
    failed.insert(key);
    if (!error.empty() && errors.size() < MAX_ERRORS
        && std::find(errors.begin(), errors.end(), error) == errors.end()) {
        errors.push_back(error);
    }
}

bool LazyChunkSource::hasPending() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return !pending.empty() || !loading.empty() || inflight > 0;
}

ChunkSource::Status LazyChunkSource::status() const
{
    std::lock_guard<std::mutex> lock(mutex);
    Status s;
    s.pending = pending.size() + loading.size() + inflight;
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

void LazyChunkSource::planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group)
{
    group.push_back(candidates[0]);
}

void LazyChunkSource::readBatch(const std::vector<ChunkKey>& keys,
    std::vector<std::shared_ptr<Chunk>>& out, std::vector<std::string>& errors)
{
    for (size_t i = 0; i < keys.size(); i++) {
        const ChunkKey& k = keys[i];
        out[i] = read(k.level, k.band, k.cx, k.cy, errors[i]);
    }
}

bool LazyChunkSource::loadSome()
{
    std::vector<ChunkKey> candidates;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (pending.empty())
            return false;
        // Already as busy as it is allowed to be: say so, so that the caller can
        // go and serve another source instead of blocking inside this one.
        if (loading.size() >= maxConcurrentReads())
            return false;
        // most wanted first: see RequestPriority
        for (const auto& e : pending) {
            candidates.push_back(e.second);
            if (candidates.size() >= BATCH_CANDIDATES)
                break;
        }
    }

    // The policy runs without the mutex, since it is allowed to look at the
    // file's geometry, and cannot do any harm from there: whatever it picks is
    // checked against the queue below.
    std::vector<ChunkKey> group;
    planBatch(candidates, group);
    if (group.empty()) {
        // a policy that came back with nothing would spin; the most wanted read
        // has to make progress
        group.push_back(candidates[0]);
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        // The queue may have moved while the policy was thinking (a frame
        // cancelling requests, another loader thread taking them), so take only
        // what is still there. This is also what rejects a key the policy made
        // up.
        std::vector<ChunkKey> kept;
        for (const ChunkKey& k : group) {
            auto q = queued.find(k);
            if (q == queued.end())
                continue;
            pending.erase({ q->second, k });
            queued.erase(q);
            loading.insert(k);
            kept.push_back(k);
        }
        if (kept.empty())
            return true; // the queue changed under us; that still counts as progress
        group.swap(kept);
    }

    // readBatch() is blocking and must not hold our mutex: fetch() has to stay
    // responsive while the disk is busy
    std::vector<std::shared_ptr<Chunk>> chunks(group.size());
    std::vector<std::string> errors(group.size());
    readBatch(group, chunks, errors);

    // The cache owns them from here, so they count against the RAM budget even
    // before anyone claims them, and may be evicted if nobody ever does. 'owner'
    // is not known yet: the Image that claims one adopts it then.
    for (const auto& chunk : chunks) {
        ChunkCache::retain(chunk, nullptr);
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (size_t i = 0; i < group.size(); i++) {
            loading.erase(group[i]);
            if (chunks[i]) {
                ready[group[i]] = chunks[i];
            } else {
                recordFailureLocked(group[i], errors[i]);
            }
        }
    }
    // A slot just freed up: a thread that found this source busy and went to
    // sleep should come back rather than wait out its timeout.
    ChunkLoader::notify();
    return true;
}

size_t LazyChunkSource::dropStaleRequests(uint64_t frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    size_t dropped = 0;
    // 'pending' is ordered newest frame first, so everything stale is a suffix
    // of it: no need to walk the part that is still wanted.
    while (!pending.empty()) {
        auto worst = std::prev(pending.end());
        if (frame - worst->first.frame < STALE_AFTER_FRAMES)
            break;
        queued.erase(worst->second);
        pending.erase(worst);
        dropped++;
    }
    return dropped;
}

namespace ChunkLoader {

static std::mutex mutex;
static std::condition_variable cv;
static bool ready = false;
static std::atomic<bool> running { false };
// Never destroyed on purpose: the app is allowed to exit brutally while a read
// is stuck in GDAL, and a static std::thread that is still joinable at exit
// would call std::terminate.
static std::vector<std::thread*> threads;
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

    std::vector<std::shared_ptr<LazyChunkSource>> alive = aliveSources();
    if (alive.empty())
        return false;

    // Each thread starts its scan where it left off, so that several threads
    // waking up together do not all go for the first image and then trickle down
    // the same list. Combined with LazyChunkSource::maxConcurrentReads, which
    // makes a busy source say "not me" instead of blocking, this is what keeps N
    // threads spread over N images.
    static thread_local size_t next = 0;

    bool didsomething = false;
    for (size_t i = 0; i < alive.size(); i++) {
        const auto& s = alive[(next + i) % alive.size()];
        if (s->loadSome()) {
            didsomething = true;
            // a chunk landed: the display has to be asked to draw again,
            // otherwise it will never come and claim it
            gActive = std::max(gActive, 2);
            next = (next + i + 1) % alive.size();
            break;
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
    // More than one, because a single thread spends most of its time inside one
    // blocking RasterIO: the point is to have several reads in flight, whether
    // the bottleneck is the disk, the network (/vsicurl/) or decompression. Reads
    // of one image only actually overlap if GDAL gave us a thread-safe handle
    // (see GDALChunkSource); otherwise the parallelism is across images.
    size_t n = gChunkLoaderThreads;
    if (n == 0) {
        unsigned hw = std::thread::hardware_concurrency();
        n = std::min<size_t>(4, hw ? hw : 1);
    }
    for (size_t i = 0; i < n; i++) {
        threads.push_back(new std::thread(run));
    }
}

void stop()
{
    running = false;
    notify();
}

void join()
{
    for (std::thread* t : threads) {
        if (t->joinable()) {
            t->join();
        }
    }
}

void notify()
{
    {
        std::lock_guard<std::mutex> lk(mutex);
        ready = true;
    }
    // every idle thread, not one: a batch of newly visible chunks is exactly when
    // they should all wake up
    cv.notify_all();
}

}

#include <doctest.h>

namespace {

// Counts reads; every chunk read succeeds.
class CountingLazySource : public LazyChunkSource {
public:
    size_t reads = 0;
    std::vector<ChunkKey> readOrder;

    std::vector<Level> describeLevels() const override
    {
        return { Level(4 * CHUNK_SIZE, CHUNK_SIZE, 1.0, 1.0) };
    }

protected:
    std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy, std::string&) override
    {
        reads++;
        readOrder.push_back({ level, band, cx, cy });
        return std::make_shared<Chunk>(CHUNK_SIZE, CHUNK_SIZE);
    }
};

// Groups every candidate that falls on the same tile, like GDALChunkSource does,
// and records the groups it was asked to serve.
class BatchingLazySource : public CountingLazySource {
public:
    std::vector<std::vector<ChunkKey>> batches;

protected:
    void planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group) override
    {
        for (const ChunkKey& k : candidates) {
            if (k.level == candidates[0].level && k.cx == candidates[0].cx
                && k.cy == candidates[0].cy) {
                group.push_back(k);
            }
        }
    }

    void readBatch(const std::vector<ChunkKey>& keys, std::vector<std::shared_ptr<Chunk>>& out,
        std::vector<std::string>& errors) override
    {
        batches.push_back(keys);
        CountingLazySource::readBatch(keys, out, errors);
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
    CHECK(src->loadSome());
    CHECK(src->reads == 1);
    CHECK(src->status().pending == 0);
    CHECK(bool(src->fetch(0, 0, 0, 0)));

    SUBCASE("a cancelled read is queued again if it comes back into view")
    {
        ChunkCache::beginFrame();
        CHECK(!src->fetch(0, 0, 1, 0));
        CHECK(src->status().pending == 1);
        CHECK(src->loadSome());
        CHECK(src->reads == 2);
    }
}

TEST_CASE("cancelling never drops a read that is being served")
{
    auto src = std::make_shared<CountingLazySource>();

    ChunkCache::beginFrame();
    CHECK(!src->fetch(0, 0, 0, 0));
    CHECK(src->loadSome()); // popped from 'pending', read, and now in 'ready'

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

TEST_CASE("reads follow the order of the newest frame, not of the first request")
{
    auto src = std::make_shared<CountingLazySource>();

    // frame F asks for three chunks, centre-outwards
    ChunkCache::beginFrame();
    src->fetch(0, 0, 0, 0);
    src->fetch(0, 0, 1, 0);
    src->fetch(0, 0, 2, 0);

    // frame F+1: the view has panned, and the same three chunks are still
    // visible but in a different order: 2 is now under the cursor and 0 at the
    // edge. Nothing new is requested, so first-request order would still serve 0.
    ChunkCache::beginFrame();
    src->fetch(0, 0, 2, 0);
    src->fetch(0, 0, 1, 0);
    src->fetch(0, 0, 0, 0);

    while (src->loadSome()) { }
    REQUIRE(src->readOrder.size() == 3);
    CHECK(src->readOrder[0].cx == 2);
    CHECK(src->readOrder[1].cx == 1);
    CHECK(src->readOrder[2].cx == 0);
}

TEST_CASE("a request renewed by the newest frame outranks an older frame's")
{
    auto src = std::make_shared<CountingLazySource>();

    ChunkCache::beginFrame();
    src->fetch(0, 0, 0, 0); // asked for in frame F, and never again

    ChunkCache::beginFrame();
    src->fetch(0, 0, 1, 0); // frame F+1 wants this one

    while (src->loadSome()) { }
    REQUIRE(src->readOrder.size() == 2);
    CHECK(src->readOrder[0].cx == 1);
    CHECK(src->readOrder[1].cx == 0);
}

TEST_CASE("the queue backstop drops the least wanted request")
{
    auto src = std::make_shared<CountingLazySource>();

    // one frame asks for far more than the queue can hold; since the display
    // asks centre-outwards, what must survive is what was asked for first
    ChunkCache::beginFrame();
    const size_t asked = 200;
    for (size_t i = 0; i < asked; i++)
        src->fetch(0, 0, i, 0);

    auto s = src->status();
    CHECK(s.pending == 64); // MAX_PENDING
    CHECK(s.failed == 0);

    // and the ones it kept are the ones asked for first, i.e. nearest the centre
    CHECK(src->loadSome());
    CHECK(src->loadSome());
    CHECK(src->loadSome());
    REQUIRE(src->readOrder.size() == 3);
    CHECK(src->readOrder[0].cx == 0);
    CHECK(src->readOrder[1].cx == 1);
    CHECK(src->readOrder[2].cx == 2);
}

TEST_CASE("all the bands of one tile are served by a single batch")
{
    auto src = std::make_shared<BatchingLazySource>();

    // what Texture::update does: it wants the three bands of a tile before it can
    // upload it, so it asks for them back to back
    ChunkCache::beginFrame();
    src->fetch(0, 0, 1, 0);
    src->fetch(0, 1, 1, 0);
    src->fetch(0, 2, 1, 0);
    // and a chunk of another tile, which must not be dragged into the batch
    src->fetch(0, 0, 2, 0);

    CHECK(src->loadSome());
    REQUIRE(src->batches.size() == 1);
    CHECK(src->batches[0].size() == 3);
    for (const ChunkKey& k : src->batches[0])
        CHECK(k.cx == 1);
    // the batch is what the chunks came from, and they are all claimable now
    CHECK(bool(src->fetch(0, 0, 1, 0)));
    CHECK(bool(src->fetch(0, 1, 1, 0)));
    CHECK(bool(src->fetch(0, 2, 1, 0)));

    CHECK(src->loadSome());
    REQUIRE(src->batches.size() == 2);
    CHECK(src->batches[1].size() == 1);
    CHECK(src->batches[1][0].cx == 2);

    CHECK(!src->loadSome());
    CHECK(src->reads == 4); // nothing was read twice
}

TEST_CASE("a batch never serves a read that is no longer queued")
{
    // a policy that grabs everything it is offered, plus a key nobody asked for
    class GreedySource : public BatchingLazySource {
    protected:
        void planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group) override
        {
            group = candidates;
            group.push_back({ 0, 0, 999, 0 }); // never requested
        }
    };
    auto src = std::make_shared<GreedySource>();

    ChunkCache::beginFrame();
    src->fetch(0, 0, 0, 0);
    src->fetch(0, 0, 1, 0);

    CHECK(src->loadSome());
    REQUIRE(src->batches.size() == 1);
    CHECK(src->batches[0].size() == 2); // the made-up key was dropped
    CHECK(src->reads == 2);
    CHECK(src->status().pending == 0);
    CHECK(src->status().failed == 0);
}

TEST_CASE("a source that is already reading hands the thread back")
{
    // a serial source (the default), i.e. one whose reads cannot overlap
    class BlockingSource : public CountingLazySource {
    public:
        std::atomic<bool> entered { false };
        std::mutex m;
        std::condition_variable cv;
        bool release = false;

    protected:
        std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
            std::string& error) override
        {
            entered = true;
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [&] { return release; });
            lk.unlock();
            return CountingLazySource::read(level, band, cx, cy, error);
        }
    };
    auto slow = std::make_shared<BlockingSource>();

    ChunkCache::beginFrame();
    slow->fetch(0, 0, 0, 0);
    slow->fetch(0, 0, 1, 0);

    // one thread is stuck in the first read
    std::thread reader([&] { slow->loadSome(); });
    while (!slow->entered) {
        std::this_thread::yield();
    }

    // a second thread must not join it: the source says it is busy, so the thread
    // is free to go and serve another image instead of blocking here
    CHECK(!slow->loadSome());
    CHECK(slow->status().pending == 2); // the queued one and the one being read

    {
        std::lock_guard<std::mutex> lock(slow->m);
        slow->release = true;
    }
    slow->cv.notify_all();
    reader.join();

    // and once it is free again the rest of the queue is served
    CHECK(slow->loadSome());
    CHECK(slow->reads == 2);
    CHECK(slow->status().pending == 0);
}

TEST_CASE("several loader threads never read the same chunk twice")
{
    // What the pool does: every thread calls loadSome() on the same source.
    class ConcurrentSource : public LazyChunkSource {
    public:
        std::mutex m;
        std::map<ChunkKey, size_t> timesRead;

        std::vector<Level> describeLevels() const override
        {
            return { Level(64 * CHUNK_SIZE, CHUNK_SIZE, 1.0, 1.0) };
        }

        // like a GDAL source with a thread-safe handle: reads may overlap
        size_t maxConcurrentReads() const override { return (size_t)-1; }

    protected:
        std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
            std::string&) override
        {
            {
                std::lock_guard<std::mutex> lock(m);
                timesRead[{ level, band, cx, cy }]++;
            }
            // small, so that 64 of them do not blow the cache budget; the size is
            // irrelevant to what is being tested
            return std::make_shared<Chunk>(4, 4);
        }
    };
    auto src = std::make_shared<ConcurrentSource>();

    ChunkCache::beginFrame();
    const size_t n = 64; // MAX_PENDING, so nothing is dropped
    for (size_t i = 0; i < n; i++) {
        src->fetch(0, 0, i, 0);
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&] { while (src->loadSome()) { } });
    }
    for (auto& t : threads) {
        t.join();
    }

    CHECK(src->timesRead.size() == n);
    for (const auto& e : src->timesRead) {
        CHECK(e.second == 1);
    }
    CHECK(src->status().pending == 0);
    CHECK(src->status().failed == 0);
}

TEST_CASE("a chunk already being read is not queued a second time")
{
    // a source that asks for the chunk it is in the middle of reading, which is
    // what another thread doing fetch() during a read amounts to
    class ReentrantSource : public CountingLazySource {
    public:
        size_t reentrantPending = 0;

    protected:
        std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
            std::string& error) override
        {
            if (reads == 0) {
                CHECK(!fetch(level, band, cx, cy));
                reentrantPending = status().pending;
            }
            return CountingLazySource::read(level, band, cx, cy, error);
        }
    };
    auto src = std::make_shared<ReentrantSource>();

    ChunkCache::beginFrame();
    src->fetch(0, 0, 0, 0);
    CHECK(src->loadSome());
    // the read in progress is the only thing outstanding: it was not re-queued
    CHECK(src->reentrantPending == 1);
    CHECK(!src->loadSome());
    CHECK(src->reads == 1);
}
