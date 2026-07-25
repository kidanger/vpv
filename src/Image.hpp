#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <imgui.h>

#include "Chunk.hpp"

using BandIndices = std::array<size_t, 3>;
#define BANDS_DEFAULT (BandIndices { 0, 1, 2 })

class Histogram;

// Statistics over an image.
//
// 'generation' is bumped every time the values change. Consumers must use it as
// the invalidation key instead of comparing min/max, because with lazy images
// the stats get refined over time (and comparing floats cannot express "the
// same numbers, but now computed over more data").
//
// 'approximate' is set when the stats were not computed over every pixel at
// full resolution (e.g. estimated from a coarse pyramid level).
//
// 'bands' records which bands the scan covered: only the bands being displayed
// are ever scanned, so that a hyperspectral cube does not read hundreds of them
// to answer a question about the three on screen (see bigimages.md). The numbers
// are therefore only valid for that selection.
//
// 'quantiles' holds the saturation cuts from SATURATIONS, computed during the
// same pass. They are precomputed rather than answered on demand because the
// values come from the configuration and nobody changes them while browsing;
// this way nothing has to be kept around between the scan and the keypress.
// Only lazy images get them: for an eager one the exact full sort is still
// affordable and is done on demand.
struct ImageStats {
    struct Quantile {
        float q, low, high;
    };

    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    uint64_t generation = 0;
    bool approximate = false;
    BandIndices bands = BANDS_DEFAULT;
    size_t level = 0; // pyramid level the scan was made at
    std::vector<Quantile> quantiles;

    void set(float min, float max, bool approximate = false)
    {
        this->min = min;
        this->max = max;
        this->approximate = approximate;
        generation++;
    }
};

struct Image {
    std::string ID;
    // The whole interleaved buffer, or nullptr for a lazy image (one whose
    // chunks are read on demand). Everything that reads it directly must be
    // unreachable when it is null; see bigimages.md.
    float* pixels;
    size_t w, h, c;
    ImVec2 size;
    // mutable for the same reason as 'levels' below: for a lazy image, reading
    // a chunk is what discovers the range of values, and reading is const
    mutable ImageStats stats;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    // A lazy image: no buffer, no statistics yet. Both are built up from the
    // chunks as they arrive.
    Image(const std::shared_ptr<ChunkSource>& source, size_t w, size_t h, size_t c);
    ~Image();

    bool isLazy() const { return !pixels; }

    // What the image costs in RAM, for the cache's budget: the interleaved
    // buffer of an eager image, and nothing at all for a lazy one. A lazy
    // image's chunks are owned and accounted for by ChunkCache, against the same
    // limit (see bigimages.md).
    size_t memoryFootprint() const;

    // Returns false if the values are not available (out of bounds, or not
    // resident yet); in that case 'values' is left untouched.
    bool getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool, 3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

    // Resolution pyramid. Level 0 is always present and is full resolution;
    // 'w', 'h', 'size' and every coordinate exposed to the GUI, the view, the
    // SVG overlays and the scripts are always level-0 pixels, whatever is
    // resident and whatever is being displayed. See bigimages.md.
    size_t getLevelCount() const { return levels.size(); }
    const Level& getLevel(size_t level) const { return levels[level]; }

    // Returns the chunk, fetching it from the source if needed, or nullptr when
    // it does not exist, is not resident yet, or has been evicted.
    //
    // 'retain' registers the chunk with ChunkCache and remembers it in the
    // residency grid. Scans that walk a large part of the image pass false, so
    // that reading statistics does not fault a planar copy of the whole image
    // into RAM (and does not push the visible chunks out of the cache).
    std::shared_ptr<Chunk> getChunk(size_t level, BandIndex band, size_t cx, size_t cy,
        bool retain = true) const;

    // Same, but waits for the read instead of queuing it: it only returns
    // nullptr when the chunk does not exist or the read failed. Blocking, so it
    // is only legal off the render thread (the chunk-loading thread, or the io
    // thread while a provider is still finishing). Used by EditChunkSource to
    // gather the input tiles of the tile it is evaluating.
    std::shared_ptr<Chunk> getChunkBlocking(size_t level, BandIndex band, size_t cx, size_t cy,
        bool retain = true) const;

    // Whether a chunk getChunk() did not return is on its way. False means
    // asking again will not help (the band does not exist, or the read failed):
    // the display uses it to know whether it is worth repainting.
    bool isChunkPending(size_t level, BandIndex band, size_t cx, size_t cy) const;

    // Visits every pixel of one band inside the half-open region
    // [x0,x1) x [y0,y1) (in pixels of 'level'), as runs of contiguous values.
    //
    // The iteration is chunk-major, so the order of the runs is unspecified:
    // only use this for order-independent reductions (min/max, histogram bins,
    // collecting values to sort). Returns false if at least one chunk was
    // missing, in which case the corresponding pixels were skipped.
    bool scanRegion(size_t level, BandIndex band, size_t x0, size_t y0, size_t x1, size_t y1,
        bool retain, const std::function<void(const float*, size_t)>& f) const;

    // Statistics, computed from the coarsest pyramid level (which for an eager
    // image is full resolution).
    //
    // Only the bands that are being displayed are scanned, so the answer is only
    // valid for that band selection, hence wantsStats().
    bool wantsStats(BandIndices bands) const;
    void markStatsRequested(BandIndices bands);
    // One of the precomputed saturation cuts. Returns false when there is none
    // for that quantile or that band selection (in particular for eager images,
    // whose quantiles are still computed exactly on demand).
    //
    // Goes through the stats lock, unlike reading stats.min/max: 'quantiles' is
    // a vector that another thread assigns to, so reading it unlocked would be a
    // crash rather than a stale float.
    bool getQuantiles(float q, BandIndices bands, float& low, float& high) const;
    // Blocking. For a lazy image this is only called from the chunk-loading
    // thread, where blocking reads are what we want; see
    // ChunkLoader::requestStats.
    void computeStats(BandIndices bands);

private:
    // 'levels' is mutable because getChunk() is a read-through cache: it can
    // populate the residency grid without changing anything observable.
    mutable std::vector<Level> levels;
    std::shared_ptr<ChunkSource> source;
    std::shared_ptr<Chunk> getChunkImpl(size_t level, BandIndex band, size_t cx, size_t cy,
        bool retain, bool blocking) const;
    // guards the chunk grids; never held while ChunkSource::fetch runs
    mutable std::mutex chunkMutex;
    mutable std::mutex statsMutex;
    // band selection a stats scan has already been asked for, so that
    // Sequence::tick does not queue one on every frame
    BandIndices statsRequested = BANDS_DEFAULT;
    bool statsEverRequested = false;
};
