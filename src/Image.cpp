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

std::shared_ptr<Chunk> Image::getChunk(size_t level, BandIndex band, size_t cx, size_t cy,
    bool retain) const
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
    if (!chunk || !retain)
        return chunk;

    std::lock_guard<std::mutex> lock(chunkMutex);
    Band& b = levels[level].bands[band];
    // another thread may have won the race; keep whichever is already there so
    // that callers holding a pointer to it stay consistent
    if (!b.chunks[index])
        b.chunks[index] = chunk;
    return b.chunks[index];
}

bool Image::scanRegion(size_t level, BandIndex band, size_t x0, size_t y0, size_t x1, size_t y1,
    bool retain, const std::function<void(const float*, size_t)>& f) const
{
    if (level >= levels.size())
        return false;
    const Level& lv = levels[level];

    x1 = std::min(x1, lv.w);
    y1 = std::min(y1, lv.h);
    if (x0 >= x1 || y0 >= y1)
        return true;

    bool complete = true;
    // chunk-major: each chunk is fetched (and possibly sliced) exactly once
    for (size_t cy = y0 / CHUNK_SIZE; cy <= (y1 - 1) / CHUNK_SIZE; cy++) {
        size_t cy0 = cy * CHUNK_SIZE;
        size_t ry0 = std::max(y0, cy0);
        size_t ry1 = std::min(y1, cy0 + CHUNK_SIZE);
        for (size_t cx = x0 / CHUNK_SIZE; cx <= (x1 - 1) / CHUNK_SIZE; cx++) {
            size_t cx0 = cx * CHUNK_SIZE;
            size_t rx0 = std::max(x0, cx0);
            size_t rx1 = std::min(x1, cx0 + CHUNK_SIZE);

            std::shared_ptr<Chunk> chunk = getChunk(level, band, cx, cy, retain);
            if (!chunk) {
                complete = false;
                continue;
            }
            for (size_t y = ry0; y < ry1; y++) {
                f(&chunk->pixels[(y - cy0) * chunk->w + (rx0 - cx0)], rx1 - rx0);
            }
        }
    }
    return complete;
}

Image::~Image()
{
    free(pixels);
}

bool Image::getPixelValueAt(size_t x, size_t y, float* values, size_t d) const
{
    if (x >= w || y >= h)
        return false;

    // The probed pixel is almost always inside a chunk the display already
    // uploaded, so retaining is free here.
    size_t cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE;
    size_t lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE;
    for (size_t i = 0; i < d && i < c; i++) {
        std::shared_ptr<Chunk> chunk = getChunk(0, i, cx, cy);
        if (!chunk)
            return false;
        values[i] = chunk->pixels[ly * chunk->w + lx];
    }
    return true;
}

std::array<bool, 3> Image::getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const
{
    std::array<bool, 3> valids { false, false, false };
    if (x >= w || y >= h)
        return valids;

    size_t cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE;
    size_t lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE;
    for (size_t i = 0; i < 3; i++) {
        size_t b = bands[i];
        if (b >= c)
            continue;
        std::shared_ptr<Chunk> chunk = getChunk(0, b, cx, cy);
        if (!chunk)
            continue;
        values[i] = chunk->pixels[ly * chunk->w + lx];
        valids[i] = true;
    }
    return valids;
}

#include <doctest.h>

// Chunk reads must be indistinguishable from reading the interleaved buffer.
// This is checkable today because everything is resident; once GDAL sources
// arrive it no longer is, which is why these conversions were done first.
TEST_CASE("Image chunk access matches the raw buffer")
{
    const size_t w = CHUNK_SIZE + 37, h = 3, c = 2;
    float* pixels = (float*)malloc(sizeof(float) * w * h * c);
    for (size_t i = 0; i < w * h; i++) {
        pixels[i * c + 0] = (float)i;
        pixels[i * c + 1] = -(float)i;
    }
    Image img(pixels, w, h, c);

    SUBCASE("getPixelValueAt")
    {
        for (size_t y = 0; y < h; y++) {
            for (size_t x : { (size_t)0, (size_t)1, CHUNK_SIZE - 1, CHUNK_SIZE, w - 1 }) {
                float v[2] = { 42.f, 42.f };
                REQUIRE(img.getPixelValueAt(x, y, v, c));
                CHECK(v[0] == pixels[(y * w + x) * c + 0]);
                CHECK(v[1] == pixels[(y * w + x) * c + 1]);
            }
        }
        float v[2];
        CHECK(img.getPixelValueAt(w, 0, v, c) == false);
        CHECK(img.getPixelValueAt(0, h, v, c) == false);
    }

    SUBCASE("getPixelValueAtBands")
    {
        float v[3] = { 0, 0, 0 };
        auto valids = img.getPixelValueAtBands(CHUNK_SIZE + 1, 2, BANDS_DEFAULT, v);
        CHECK(valids[0] == true);
        CHECK(valids[1] == true);
        CHECK(valids[2] == false); // only two bands
        CHECK(v[0] == pixels[(2 * w + CHUNK_SIZE + 1) * c + 0]);
        CHECK(v[1] == pixels[(2 * w + CHUNK_SIZE + 1) * c + 1]);
    }

    SUBCASE("scanRegion covers exactly the region, once")
    {
        // spans the chunk boundary and clips on the right
        size_t x0 = CHUNK_SIZE - 3, y0 = 1, x1 = w, y1 = h;
        std::vector<float> got;
        bool complete = img.scanRegion(0, 0, x0, y0, x1, y1, false,
            [&](const float* run, size_t n) { got.insert(got.end(), run, run + n); });
        CHECK(complete == true);

        std::vector<float> expected;
        for (size_t y = y0; y < y1; y++)
            for (size_t x = x0; x < x1; x++)
                expected.push_back(pixels[(y * w + x) * c + 0]);

        // the iteration order is chunk-major, so compare as multisets
        std::sort(got.begin(), got.end());
        std::sort(expected.begin(), expected.end());
        CHECK(got == expected);
    }

    SUBCASE("scanRegion clamps and tolerates empty regions")
    {
        size_t calls = 0;
        auto count = [&](const float*, size_t) { calls++; };
        CHECK(img.scanRegion(0, 0, 0, 0, 0, 0, false, count) == true);
        CHECK(calls == 0);
        CHECK(img.scanRegion(0, 0, 5, 5, 1, 1, false, count) == true);
        CHECK(calls == 0);
        // a band that does not exist yields nothing and reports incomplete
        CHECK(img.scanRegion(0, 7, 0, 0, w, h, false, count) == false);
        CHECK(calls == 0);
        // clamped to the image
        CHECK(img.scanRegion(0, 0, 0, 0, w * 2, h * 2, false, count) == true);
        CHECK(calls > 0);
    }

    SUBCASE("retain=false does not populate the residency grid")
    {
        size_t calls = 0;
        img.scanRegion(0, 0, 0, 0, w, h, false, [&](const float*, size_t) { calls++; });
        CHECK(calls > 0);
        // nothing was kept, so a peek-like call still has to go to the source;
        // observable only through memory, so just check we can scan again
        std::vector<float> a, b;
        img.scanRegion(0, 1, 0, 0, w, h, false,
            [&](const float* r, size_t n) { a.insert(a.end(), r, r + n); });
        img.scanRegion(0, 1, 0, 0, w, h, true,
            [&](const float* r, size_t n) { b.insert(b.end(), r, r + n); });
        CHECK(a == b);
    }
}
