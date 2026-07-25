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
// 'sample' holds a bounded, sorted selection of the values the scan saw, so that
// quantiles can be answered without re-reading anything. It is only filled in
// for a lazy image: an eager one is entirely in RAM, so its quantiles are still
// computed exactly. 'bands' records which bands the scan covered, because for a
// lazy image only the bands being displayed are read (see bigimages.md) and the
// numbers are therefore only valid for that selection.
struct ImageStats {
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    uint64_t generation = 0;
    bool approximate = false;
    std::vector<float> sample; // sorted, finite values only
    BandIndices bands = BANDS_DEFAULT;
    size_t level = 0; // pyramid level the scan was made at

    void set(float min, float max, bool approximate = false)
    {
        this->min = min;
        this->max = max;
        this->approximate = approximate;
        generation++;
    }

    // Symmetric quantile cut over 'sample'. Returns false when there is no
    // sample to answer from (an eager image, or nothing scanned yet).
    bool quantiles(float q, float& low, float& high) const
    {
        if (sample.empty())
            return false;
        size_t n = sample.size();
        low = sample[std::min(n - 1, (size_t)(q * n))];
        high = sample[std::min(n - 1, (size_t)((1 - q) * n))];
        return true;
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

    // What the image costs in RAM, for the cache's budget. A lazy image reports
    // only its resident chunks, which is *not* tracked as they arrive: the
    // cache's running total is updated on store/remove only. Step 6 replaces
    // this with a real chunk budget.
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
    // it does not exist or is not resident yet.
    //
    // 'retain' keeps the chunk in the residency grid. Scans that walk a large
    // part of the image pass false, so that reading statistics does not fault a
    // planar copy of the whole image into RAM next to Image::pixels.
    std::shared_ptr<Chunk> getChunk(size_t level, BandIndex band, size_t cx, size_t cy,
        bool retain = true) const;

    // Visits every pixel of one band inside the half-open region
    // [x0,x1) x [y0,y1) (in pixels of 'level'), as runs of contiguous values.
    //
    // The iteration is chunk-major, so the order of the runs is unspecified:
    // only use this for order-independent reductions (min/max, histogram bins,
    // collecting values to sort). Returns false if at least one chunk was
    // missing, in which case the corresponding pixels were skipped.
    bool scanRegion(size_t level, BandIndex band, size_t x0, size_t y0, size_t x1, size_t y1,
        bool retain, const std::function<void(const float*, size_t)>& f) const;

    // Statistics of a lazy image, computed from the coarsest pyramid level.
    //
    // Only the bands that are being displayed are scanned, so that a
    // hyperspectral cube does not read hundreds of bands to answer a question
    // about the three on screen. The flip side is that the answer is only valid
    // for that band selection, hence wantsStats().
    bool wantsStats(BandIndices bands) const;
    void markStatsRequested(BandIndices bands);
    // Quantile cut over the sample the statistics pass left behind, for the
    // given band selection. Returns false when there is nothing to answer from
    // (the pass has not run, or ran for other bands).
    //
    // Goes through the stats lock, unlike reading stats.min/max: 'sample' is a
    // vector that the loader thread assigns to, so reading it unlocked would be
    // a crash rather than a stale float.
    bool getQuantiles(float q, BandIndices bands, float& low, float& high) const;
    // Blocking; only ever called from the chunk-loading thread, where blocking
    // reads are what we want (see ChunkLoader::requestStats).
    void computeStats(BandIndices bands);

private:
    // 'levels' is mutable because getChunk() is a read-through cache: it can
    // populate the residency grid without changing anything observable.
    mutable std::vector<Level> levels;
    std::shared_ptr<ChunkSource> source;
    // guards the chunk grids; never held while ChunkSource::fetch runs
    mutable std::mutex chunkMutex;
    mutable std::mutex statsMutex;
    // band selection a stats scan has already been asked for, so that
    // Sequence::tick does not queue one on every frame
    BandIndices statsRequested = BANDS_DEFAULT;
    bool statsEverRequested = false;
};
