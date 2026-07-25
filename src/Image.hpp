#pragma once

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
struct ImageStats {
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    uint64_t generation = 0;
    bool approximate = false;

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
    float* pixels;
    size_t w, h, c;
    ImVec2 size;
    ImageStats stats;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    ~Image();

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

private:
    // 'levels' is mutable because getChunk() is a read-through cache: it can
    // populate the residency grid without changing anything observable.
    mutable std::vector<Level> levels;
    std::shared_ptr<ChunkSource> source;
    // guards the chunk grids; never held while ChunkSource::fetch runs
    mutable std::mutex chunkMutex;
};
