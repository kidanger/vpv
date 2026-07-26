#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "Chunk.hpp"

struct Image;

// See bigimages.md.
//
// A LazyChunkSource never blocks the thread that asks for a chunk: fetch()
// either hands back a chunk that has already been read, or queues the read and
// returns nullptr. The reads themselves happen on the single chunk-loading
// thread below (GDALDataset is not thread-safe, so there is exactly one).

struct ChunkKey {
    size_t level = 0;
    BandIndex band = 0;
    size_t cx = 0, cy = 0;

    bool operator<(const ChunkKey& o) const
    {
        return std::tie(level, band, cx, cy) < std::tie(o.level, o.band, o.cx, o.cy);
    }
    bool operator==(const ChunkKey& o) const
    {
        return std::tie(level, band, cx, cy) == std::tie(o.level, o.band, o.cx, o.cy);
    }
};

// How badly a queued read is wanted. The display asks for its visible chunks
// centre-outwards on every frame, so within one frame the order the requests
// came in *is* the distance to the centre, and a request still carrying an older
// frame is one this frame has not renewed yet.
//
// Ordered newest frame first, then request order inside the frame: the front of
// the queue is what the newest frame wanted most, the back is the least wanted
// thing of the oldest frame still queued.
struct RequestPriority {
    uint64_t frame = 0;
    uint64_t seq = 0;

    bool operator<(const RequestPriority& o) const
    {
        if (frame != o.frame)
            return frame > o.frame;
        return seq < o.seq;
    }
};

class LazyChunkSource : public ChunkSource {
public:
    // Non-blocking. Returns a previously read chunk, or nullptr after having
    // queued the read.
    std::shared_ptr<Chunk> fetch(size_t level, BandIndex band, size_t cx, size_t cy) final;

    // Reads the chunk right now. Only called from the chunk-loading thread. A
    // chunk read this way is *not* put in 'ready': the statistics pass walks a
    // whole level once and nobody else is waiting for those chunks, so keeping
    // them would be pure memory growth. Failures are remembered, though, so a
    // band that does not exist is not read again.
    std::shared_ptr<Chunk> fetchBlocking(size_t level, BandIndex band, size_t cx, size_t cy) final;

    // Reads at most one batch of queued chunks. Returns false when there was
    // nothing to do. Only ever called from a chunk-loading thread.
    bool loadSome();

    // Forgets every queued read that nobody has asked for since frame
    // 'frame - STALE_AFTER_FRAMES'. Since the display re-asks for each of its
    // visible chunks on every frame, "not asked for lately" means "no longer
    // wanted": panning away from a chunk is enough to cancel its read. Returns
    // how many were dropped. Called once per frame from ChunkLoader::beginFrame.
    size_t dropStaleRequests(uint64_t frame);

    bool hasPending() const;

    // Queued/in-flight and permanently failed counts, plus the distinct error
    // messages. Cheap enough to be called once per frame per window.
    Status status() const final;

    // Whether a chunk that fetch() did not produce is still going to arrive.
    // Everything except a read that failed for good is: the display re-asks for
    // every visible chunk on every frame.
    bool isPending(size_t level, BandIndex band, size_t cx, size_t cy) const final;

protected:
    // Actually read the chunk. Blocking. Returns nullptr if the chunk cannot be
    // produced at all, in which case it will not be asked for again; 'error'
    // should then say why, for the indicator's tooltip.
    virtual std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
        std::string& error)
        = 0;

    // Which of the queued reads one call to readBatch() should serve.
    // 'candidates' holds the most wanted reads, in priority order, and is never
    // empty; 'group' must come back holding candidates[0] (the most wanted read
    // always makes progress) and nothing that is not in 'candidates'. Called
    // without any lock held, so it may look at the file's geometry.
    //
    // The default is one read at a time, which is what every source but GDAL
    // wants: EditChunkSource evaluates one tile per call and has nothing to gain
    // from grouping.
    virtual void planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group);

    // Serve a group planned by planBatch(). 'out' and 'errors' are sized to
    // match 'keys': out[i] is the chunk for keys[i], or nullptr with errors[i]
    // saying why. Blocking, and called without any lock held.
    //
    // The default reads them one by one, so a source only has to override this
    // if it also overrode planBatch().
    virtual void readBatch(const std::vector<ChunkKey>& keys,
        std::vector<std::shared_ptr<Chunk>>& out, std::vector<std::string>& errors);

private:
    // How many reads may be outstanding. The display asks for its chunks
    // centre-outwards every frame, so the head of the queue is what matters
    // most; when the view moves faster than the disk, dropping the tail is
    // better than letting a stale wish list grow without bound. This is the
    // hard backstop; dropStaleRequests() is what normally keeps the queue to
    // what is actually on screen.
    static constexpr size_t MAX_PENDING = 64;
    // How many of the most wanted queued reads planBatch() gets to look at. Only
    // an upper bound on the work the policy does; the group it comes back with is
    // capped by the source itself.
    static constexpr size_t BATCH_CANDIDATES = 32;    // A queued read survives being unasked for this many frames. One frame of
    // grace rather than zero, because the frame a request is made in is not
    // over when the next one starts counting, and because a window that skips
    // one frame (a level change, a collapsed window redrawing) should not have
    // its whole wish list thrown away.
    static constexpr uint64_t STALE_AFTER_FRAMES = 2;
    // Distinct error messages kept for the tooltip.
    static constexpr size_t MAX_ERRORS = 4;

    // Remembers that the read of 'key' failed, and why. Takes the mutex.
    void recordFailure(const ChunkKey& key, const std::string& error);
    // Same, with the mutex already held.
    void recordFailureLocked(const ChunkKey& key, const std::string& error);

    mutable std::mutex mutex;
    // Chunks that were read but that nobody has claimed yet. Weak, like Image's
    // residency grid and for the same reason: they are owned by ChunkCache, so
    // they count against the one RAM budget and can be evicted before anyone
    // asks. An evicted one is simply read again.
    std::map<ChunkKey, std::weak_ptr<Chunk>> ready;
    // Queued reads, most wanted first. Kept in step with 'queued', which is both
    // the dedup and the reverse lookup needed to re-prioritise an entry that is
    // asked for again (its priority is part of the key, so it has to be erased
    // and reinserted).
    std::set<std::pair<RequestPriority, ChunkKey>> pending;
    std::map<ChunkKey, RequestPriority> queued;
    // Ties the order of the requests made during one frame. Never reset: it only
    // ever has to be comparable with the other requests of the same frame.
    uint64_t nextSeq = 0;
    std::set<ChunkKey> failed; // read() said no; do not ask again
    // Reads that have been taken off 'pending' and are being served right now.
    // Kept apart from 'queued' so that fetch() can tell "still queued, may be
    // re-prioritised" from "already being read, nothing to do but wait": a read
    // that has started is not interruptible, and must not be queued a second
    // time.
    std::set<ChunkKey> loading;
    // Blocking reads in progress (fetchBlocking), which are not in 'loading'
    // because nobody else may join them: counted only so that the indicator does
    // not blink off between two chunks.
    size_t inflight = 0;
    std::vector<std::string> errors;
};

namespace ChunkLoader {

// Sources are held weakly: an Image dropping its source is enough to stop the
// loader from touching it.
void add(const std::shared_ptr<LazyChunkSource>& source);

// Ask for an image's statistics to be computed (Image::computeStats). Runs on
// the loader thread, where reads block, so the pass needs no retry logic.
//
// Statistics jobs are served *before* any display chunk: the pass is small and
// bounded, and until it lands the colormap has nothing to initialise itself
// with, so panning cannot be allowed to starve it. The image is held weakly.
void requestStats(const std::shared_ptr<Image>& image, std::array<size_t, 3> bands);

// Called once per frame from the main loop, right after ChunkCache::beginFrame.
// Drops the queued reads nobody has asked for lately: see
// LazyChunkSource::dropStaleRequests. A read that has already started is not
// interruptible, so at most one out-of-view chunk is still paid for.
void beginFrame();

void start();
void stop();
void join();

// Wake the loader up; called whenever a chunk is queued.
void notify();

}
