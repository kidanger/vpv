#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "Histogram.hpp"
#include "Image.hpp"

Image::Image(float* pixels, size_t w, size_t h, size_t c)
    : pixels(pixels)
    , w(w)
    , h(h)
    , c(c)
    , lastUsed(0)
    , histogram(std::make_shared<Histogram>())
{
    static int id = 0;
    id++;
    ID = "Image " + std::to_string(id);

    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < w * h * c; i++) {
        float v = pixels[i];
        min = std::min(min, v);
        max = std::max(max, v);
    }
    if (!std::isfinite(min) || !std::isfinite(max)) {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < w * h * c; i++) {
            float v = pixels[i];
            if (std::isfinite(v)) {
                min = std::min(min, v);
                max = std::max(max, v);
            }
        }
    }
    stats.set(min, max);
    size = ImVec2(w, h);

    // For now every image is a single, fully-resident level backed by the
    // interleaved buffer we were handed. Lazy GDAL images will add levels and
    // replace the source (steps 3 and 4).
    levels.emplace_back(w, h, 1.0);
    source = std::make_shared<InRamChunkSource>(pixels, w, h, c);
}

std::shared_ptr<Chunk> Image::getChunk(size_t level, BandIndex band, size_t cx, size_t cy)
{
    if (level >= levels.size())
        return nullptr;
    const Level& lv = levels[level];
    if (cx >= lv.cw() || cy >= lv.ch())
        return nullptr;

    size_t index = cy * lv.cw() + cx;

    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        Band& b = levels[level].bands[band];
        if (b.chunks.empty())
            b.chunks.resize(lv.cw() * lv.ch());
        if (b.chunks[index])
            return b.chunks[index];
    }

    if (!source)
        return nullptr;

    // fetched without the lock: a lazy source is allowed to block here
    std::shared_ptr<Chunk> chunk = source->fetch(level, band, cx, cy);
    if (!chunk)
        return nullptr;

    std::lock_guard<std::mutex> lock(chunkMutex);
    Band& b = levels[level].bands[band];
    // another thread may have won the race; keep whichever is already there so
    // that callers holding a pointer to it stay consistent
    if (!b.chunks[index])
        b.chunks[index] = chunk;
    return b.chunks[index];
}

Image::~Image()
{
    free(pixels);
}

bool Image::getPixelValueAt(size_t x, size_t y, float* values, size_t d) const
{
    if (x >= w || y >= h)
        return false;

    const float* data = (float*)pixels + (w * y + x) * c;
    const float* end = (float*)pixels + (w * h) * c;
    for (size_t i = 0; i < d; i++) {
        if (data + i >= end)
            break;
        values[i] = data[i];
    }
    return true;
}

std::array<bool, 3> Image::getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const
{
    std::array<bool, 3> valids { false, false, false };
    if (x >= w || y >= h)
        return valids;

    const float* data = (float*)pixels + (w * y + x) * c;
    for (size_t i = 0; i < 3; i++) {
        size_t b = bands[i];
        if (b >= c)
            continue;
        values[i] = data[b];
        valids[i] = true;
    }
    return valids;
}
