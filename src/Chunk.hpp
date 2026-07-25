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
// from GDAL overviews. 'scale' is how many level-0 pixels one level pixel
// covers, and is *not* assumed to be a power of two.
struct Level {
    size_t w = 0, h = 0;
    double scale = 1.0;
    std::map<BandIndex, Band> bands;

    Level() = default;
    Level(size_t w, size_t h, double scale)
        : w(w)
        , h(h)
        , scale(scale)
    {
    }

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
