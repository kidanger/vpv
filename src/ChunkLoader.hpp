#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>

#include "Chunk.hpp"

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

class LazyChunkSource : public ChunkSource {
public:
    // Non-blocking. Returns a previously read chunk, or nullptr after having
    // queued the read.
    std::shared_ptr<Chunk> fetch(size_t level, BandIndex band, size_t cx, size_t cy) final;

    // Reads at most one queued chunk. Returns false when there was nothing to
    // do. Only ever called from the chunk-loading thread.
    bool loadOne();

    bool hasPending() const;

protected:
    // Actually read the chunk. Blocking. Returns nullptr if the chunk cannot be
    // produced at all, in which case it will not be asked for again.
    virtual std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy) = 0;

private:
    // How many reads may be outstanding. The display asks for its chunks
    // centre-outwards every frame, so the head of the queue is what matters
    // most; when the view moves faster than the disk, dropping the tail is
    // better than letting a stale wish list grow without bound.
    static constexpr size_t MAX_PENDING = 64;

    mutable std::mutex mutex;
    std::map<ChunkKey, std::shared_ptr<Chunk>> ready;
    std::deque<ChunkKey> pending;
    std::set<ChunkKey> queued; // dedup for 'pending'
    std::set<ChunkKey> failed; // read() said no; do not ask again
};

namespace ChunkLoader {

// Sources are held weakly: an Image dropping its source is enough to stop the
// loader from touching it.
void add(const std::shared_ptr<LazyChunkSource>& source);

void start();
void stop();
void join();

// Wake the loader up; called whenever a chunk is queued.
void notify();

}
