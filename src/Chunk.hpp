#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <vector>

// See bigimages.md.
//
// A Chunk is the unit of residency: CHUNK_SIZE x CHUNK_SIZE, planar,
// single-band. Planar (as opposed to interleaved) because it is GDAL's natural
// read unit, and because it lets a band re-selection reuse chunks that are
// already resident.
//
// The chunk grid is also the texture-tile grid (see Texture.cpp), so that a
// chunk maps to exactly one glTexSubImage2D.

using BandIndex = size_t;

static constexpr size_t CHUNK_SIZE = 1024;

struct Chunk {
    size_t w, h;
    std::vector<float> pixels; // w*h, row-major

    Chunk(size_t w, size_t h)
        : w(w)
        , h(h)
        , pixels(w * h)
    {
    }

    size_t bytes() const { return pixels.size() * sizeof(float); }
};

struct Band {
    // cw*ch slots, row-major; a null slot is a chunk that is not resident
    std::vector<std::shared_ptr<Chunk>> chunks;
};

// One resolution of an image. Level 0 is full resolution; coarser levels come
// from GDAL overviews. 'scaleX'/'scaleY' are how many level-0 pixels one level
// pixel covers, and are *not* assumed to be powers of two.
//
// The two axes are kept separate because an overview's dimensions are rounded
// independently: a 101x51 image has a 51x26 half-overview, i.e. 1.980 by 1.962.
// Using one scale for both would misplace the right or bottom edge.
struct Level {
    size_t w = 0, h = 0;
    double scaleX = 1.0, scaleY = 1.0;
    std::map<BandIndex, Band> bands;

    Level() = default;
    Level(size_t w, size_t h, double scaleX, double scaleY)
        : w(w)
        , h(h)
        , scaleX(scaleX)
        , scaleY(scaleY)
    {
    }

    // How many level-0 pixels one level pixel covers, for deciding whether this
    // level would have to be magnified. The larger axis is the conservative
    // answer.
    double scale() const { return std::max(scaleX, scaleY); }

    size_t cw() const { return (w + CHUNK_SIZE - 1) / CHUNK_SIZE; }
    size_t ch() const { return (h + CHUNK_SIZE - 1) / CHUNK_SIZE; }

    size_t chunkWidth(size_t cx) const { return std::min(CHUNK_SIZE, w - cx * CHUNK_SIZE); }
    size_t chunkHeight(size_t cy) const { return std::min(CHUNK_SIZE, h - cy * CHUNK_SIZE); }
};

class ChunkSource {
public:
    virtual ~ChunkSource() = default;

    // Produce one chunk. Returns nullptr when the chunk cannot be produced at
    // all (e.g. the band does not exist). A lazy source may also return nullptr
    // when the chunk is simply not available yet, having queued the read.
    //
    // Called without any of Image's locks held, so it is allowed to block.
    virtual std::shared_ptr<Chunk> fetch(size_t level, BandIndex band, size_t cx, size_t cy) = 0;

    // Same, but allowed to block until the chunk has actually been read. Only
    // legal on the chunk-loading thread: it is what lets the statistics pass
    // walk a whole pyramid level without a retry loop. For an already-resident
    // source this is just fetch().
    virtual std::shared_ptr<Chunk> fetchBlocking(size_t level, BandIndex band, size_t cx, size_t cy)
    {
        return fetch(level, band, cx, cy);
    }

    // The resolution pyramid this source can serve, level 0 (full resolution)
    // first, in order of increasing scale. An empty result means "only full
    // resolution", which is what every source but GDAL returns.
    virtual std::vector<Level> describeLevels() const { return {}; }
};

// Adapter for the whole-image providers (iio, png, jpeg, tiff, raw, npy, vpp):
// the buffer is already fully resident and interleaved, so chunks are sliced
// out of it on demand. Non-owning; the Image owns the buffer.
class InRamChunkSource : public ChunkSource {
    const float* pixels;
    size_t w, h, c;

public:
    InRamChunkSource(const float* pixels, size_t w, size_t h, size_t c)
        : pixels(pixels)
        , w(w)
        , h(h)
        , c(c)
    {
    }

    std::shared_ptr<Chunk> fetch(size_t level, BandIndex band, size_t cx, size_t cy) override;
};
