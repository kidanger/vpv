#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "ChunkCache.hpp"
#include "Histogram.hpp"
#include "Image.hpp"
#include "globals.hpp"

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

    size = ImVec2(w, h);

    // No scan here: statistics are computed per band selection, like a lazy
    // image's, because that is the only thing the display ever needs and it
    // keeps one code path (see bigimages.md). 'stats' therefore starts at
    // generation 0, meaning "nothing known yet".

    // For now every eager image is a single, fully-resident level backed by the
    // interleaved buffer we were handed. Only lazy GDAL images have a pyramid:
    // an eager image is already in RAM, so coarser levels would cost reads and
    // memory for something the GPU's mipmaps already handle.
    levels.emplace_back(w, h, 1.0, 1.0);
    source = std::make_shared<InRamChunkSource>(pixels, w, h, c);
}

Image::Image(const std::shared_ptr<ChunkSource>& source, size_t w, size_t h, size_t c)
    : pixels(nullptr)
    , w(w)
    , h(h)
    , c(c)
    , lastUsed(0)
    , histogram(std::make_shared<Histogram>())
    , source(source)
{
    static int id = 0;
    id++;
    ID = "Lazy image " + std::to_string(id);

    size = ImVec2(w, h);

    // The source knows its own pyramid (GDAL overviews). Anything it reports is
    // taken as-is, except that level 0 is always full resolution: the whole of
    // vpv works in level-0 pixels.
    levels = source ? source->describeLevels() : std::vector<Level>();
    if (levels.empty() || levels[0].w != w || levels[0].h != h) {
        levels.clear();
        levels.emplace_back(w, h, 1.0, 1.0);
    }

    // 'stats' is deliberately left at generation 0, meaning "nothing known
    // yet". Consumers must check that before using min/max; computeStats()
    // fills it in from the coarsest level, on the chunk-loading thread.
}

size_t Image::memoryFootprint() const
{
    // A lazy image owns nothing: its chunks belong to ChunkCache, which accounts
    // for them globally and evicts them on its own. Reporting them here would
    // count them twice, and would be a lie by the time anyone read it.
    return pixels ? w * h * c * sizeof(float) : 0;
}

// Statistics come from one pass over the coarsest pyramid level, which for an
// eager image is full resolution and for a lazy one is the cheapest place to get
// a genuinely global answer: it is usually a chunk or two, and it covers the
// whole extent instead of whatever happens to be on screen.
//
// Only the bands being displayed are scanned. Keeping the chunk grids per band
// exists precisely so that a 200-band cube does not pay for 200 reads to answer
// a question about the three bands on screen, and scanning everything would have
// thrown that away. The consequence is that the numbers are the range of the
// *selection*, which is what wantsStats() checks for.
//
// For a lazy image the number of pixels read per band is capped, because
// 'gdaladdo 2 4' on a 40k raster leaves a 10k x 10k coarsest level; above the
// budget whole chunks are skipped in a regular grid. An eager image is already
// in RAM, so it is always scanned exactly.
bool Image::wantsStats(BandIndices bands) const
{
    std::lock_guard<std::mutex> lock(statsMutex);
    if (statsEverRequested && statsRequested == bands)
        return false;
    return !stats.generation || stats.bands != bands;
}

void Image::markStatsRequested(BandIndices bands)
{
    std::lock_guard<std::mutex> lock(statsMutex);
    statsRequested = bands;
    statsEverRequested = true;
}

bool Image::getQuantiles(float q, BandIndices bands, float& low, float& high) const
{
    std::lock_guard<std::mutex> lock(statsMutex);
    if (!stats.generation || stats.bands != bands)
        return false;
    for (const ImageStats::Quantile& s : stats.quantiles) {
        if (s.q == q) {
            low = s.low;
            high = s.high;
            return true;
        }
    }
    return false;
}

void Image::computeStats(BandIndices bands)
{
    if (levels.empty())
        return;

    const size_t level = levels.size() - 1;
    const Level& lv = levels[level];

    std::vector<BandIndex> tolook;
    for (size_t i = 0; i < 3; i++) {
        if (bands[i] < c && std::find(tolook.begin(), tolook.end(), bands[i]) == tolook.end())
            tolook.push_back(bands[i]);
    }
    if (tolook.empty())
        return;

    // Skip chunks in a regular grid if the level is too big to read whole. Never
    // for an eager image: its pixels are already there.
    size_t stride = 1;
    double budget = (double)gStatsMaxPixels;
    double levelPixels = (double)lv.w * lv.h;
    if (isLazy() && budget > 0 && levelPixels > budget) {
        stride = (size_t)std::ceil(std::sqrt(levelPixels / budget));
    }

    // The saturation cuts are computed here, in the one pass that already reads
    // the pixels, so that nothing has to be kept between the scan and the
    // keypress. Only for a lazy image: an eager one would mean sorting a copy of
    // the whole image at every open, and it can still afford the exact answer on
    // demand.
    const bool wantquantiles = isLazy() && !gSaturations.empty();
    std::vector<float> values;

    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();

    for (BandIndex band : tolook) {
        for (size_t cy = 0; cy < lv.ch(); cy += stride) {
            for (size_t cx = 0; cx < lv.cw(); cx += stride) {
                // blocking: for a lazy image we are on the loader thread, which
                // is exactly why the pass lives there
                std::shared_ptr<Chunk> chunk = source->fetchBlocking(level, band, cx, cy);
                if (!chunk)
                    continue;
                for (float v : chunk->pixels) {
                    if (!std::isfinite(v))
                        continue;
                    min = std::min(min, v);
                    max = std::max(max, v);
                    if (wantquantiles)
                        values.push_back(v);
                }
            }
        }
    }

    if (min > max)
        return; // nothing finite anywhere; leave 'nothing known yet'

    std::vector<ImageStats::Quantile> quantiles;
    if (!values.empty()) {
        std::sort(values.begin(), values.end());
        size_t n = values.size();
        for (float q : gSaturations) {
            quantiles.push_back({ q,
                values[std::min(n - 1, (size_t)(q * n))],
                values[std::min(n - 1, (size_t)((1 - q) * n))] });
        }
    }

    std::lock_guard<std::mutex> lock(statsMutex);
    stats.quantiles = std::move(quantiles);
    stats.bands = bands;
    stats.level = level;
    // 'approximate' unless we read every pixel of every band at full resolution
    stats.set(min, max, level != 0 || stride != 1 || tolook.size() != c);
}

std::shared_ptr<Chunk> Image::getChunk(size_t level, BandIndex band, size_t cx, size_t cy,
    bool retain) const
{
    return getChunkImpl(level, band, cx, cy, retain, false);
}

std::shared_ptr<Chunk> Image::getChunkBlocking(size_t level, BandIndex band, size_t cx, size_t cy,
    bool retain) const
{
    return getChunkImpl(level, band, cx, cy, retain, true);
}

std::shared_ptr<Chunk> Image::getChunkImpl(size_t level, BandIndex band, size_t cx, size_t cy,
    bool retain, bool blocking) const
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
        // the slot is weak: an empty one is a chunk that was never read, or one
        // the cache has evicted since. Both mean "read it again".
        if (std::shared_ptr<Chunk> chunk = b.chunks[index].lock()) {
            // outside the lock would be tidier, but ChunkCache never takes any
            // of ours, so the order chunkMutex -> ChunkCache is safe and this
            // keeps the hot path to one function
            ChunkCache::retain(chunk, this);
            return chunk;
        }
    }

    if (!source)
        return nullptr;

    // fetched without the lock: a lazy source is allowed to block here
    std::shared_ptr<Chunk> chunk = blocking
        ? source->fetchBlocking(level, band, cx, cy)
        : source->fetch(level, band, cx, cy);
    if (!chunk)
        return nullptr;
    if (!retain)
        return chunk;

    std::lock_guard<std::mutex> lock(chunkMutex);
    Band& b = levels[level].bands[band];
    // another thread may have won the race; keep whichever is already there so
    // that callers holding a pointer to it stay consistent
    if (std::shared_ptr<Chunk> existing = b.chunks[index].lock()) {
        ChunkCache::retain(existing, this);
        return existing;
    }
    b.chunks[index] = chunk;
    // the cache is what keeps it alive from now on
    ChunkCache::retain(chunk, this);
    return chunk;
}

bool Image::isChunkPending(size_t level, BandIndex band, size_t cx, size_t cy) const
{
    return source ? source->isPending(level, band, cx, cy) : false;
}

ChunkSource::Status Image::sourceStatus() const
{
    return source ? source->status() : ChunkSource::Status {};
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
    // our chunks are the cache's; nothing else will drop them
    ChunkCache::forget(this);
    free(pixels);
}

std::shared_ptr<Chunk> Image::getResidentChunk(size_t level, BandIndex band, size_t cx,
    size_t cy) const
{
    if (level >= levels.size())
        return nullptr;
    const Level& lv = levels[level];
    if (cx >= lv.cw() || cy >= lv.ch())
        return nullptr;

    std::lock_guard<std::mutex> lock(chunkMutex);
    auto it = levels[level].bands.find(band);
    if (it == levels[level].bands.end())
        return nullptr;
    const Band& b = it->second;
    if (b.chunks.empty())
        return nullptr;
    std::shared_ptr<Chunk> chunk = b.chunks[cy * lv.cw() + cx].lock();
    if (chunk) {
        // touching it keeps the chunk under the mouse from being the one eviction
        // picks, exactly as getChunk() does
        ChunkCache::retain(chunk, this);
    }
    return chunk;
}

bool Image::locate(size_t x, size_t y, size_t level, size_t& cx, size_t& cy,
    size_t& lx, size_t& ly) const
{
    if (level >= levels.size())
        return false;
    const Level& lv = levels[level];
    if (lv.w == 0 || lv.h == 0)
        return false;

    // level-0 coordinates are canonical everywhere else, so the conversion
    // happens here and nowhere else
    size_t gx = std::min((size_t)((double)x / lv.scaleX), lv.w - 1);
    size_t gy = std::min((size_t)((double)y / lv.scaleY), lv.h - 1);
    cx = gx / CHUNK_SIZE;
    cy = gy / CHUNK_SIZE;
    lx = gx % CHUNK_SIZE;
    ly = gy % CHUNK_SIZE;
    return true;
}

bool Image::getPixelValueAt(size_t x, size_t y, float* values, size_t d, size_t level) const
{
    if (x >= w || y >= h)
        return false;

    size_t cx, cy, lx, ly;
    if (!locate(x, y, level, cx, cy, lx, ly))
        return false;

    // The probed pixel is almost always inside a chunk the display already
    // uploaded, so retaining is free here.
    for (size_t i = 0; i < d && i < c; i++) {
        std::shared_ptr<Chunk> chunk = isLazy()
            ? getResidentChunk(level, i, cx, cy)
            : getChunk(level, i, cx, cy);
        if (!chunk)
            return false;
        values[i] = chunk->pixels[ly * chunk->w + lx];
    }
    return true;
}

std::array<bool, 3> Image::getPixelValueAtBands(size_t x, size_t y, BandIndices bands,
    float* values, size_t level) const
{
    std::array<bool, 3> valids { false, false, false };
    if (x >= w || y >= h)
        return valids;

    size_t cx, cy, lx, ly;
    if (!locate(x, y, level, cx, cy, lx, ly))
        return valids;

    for (size_t i = 0; i < 3; i++) {
        size_t b = bands[i];
        if (b >= c)
            continue;
        std::shared_ptr<Chunk> chunk = isLazy()
            ? getResidentChunk(level, b, cx, cy)
            : getChunk(level, b, cx, cy);
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

namespace {

// A pyramid of constant-valued levels: each level reports a different value, so
// a chunk read at level N is recognisable.
class StepPyramidSource : public ChunkSource {
    std::vector<Level> levels;

public:
    explicit StepPyramidSource(const std::vector<Level>& levels)
        : levels(levels)
    {
    }

    std::shared_ptr<Chunk> fetch(size_t level, BandIndex band, size_t cx, size_t cy) override
    {
        if (level >= levels.size() || band != 0)
            return nullptr;
        const Level& lv = levels[level];
        if (cx >= lv.cw() || cy >= lv.ch())
            return nullptr;
        auto chunk = std::make_shared<Chunk>(lv.chunkWidth(cx), lv.chunkHeight(cy));
        std::fill(chunk->pixels.begin(), chunk->pixels.end(), (float)level);
        return chunk;
    }

    std::vector<Level> describeLevels() const override { return levels; }
};

}

TEST_CASE("a lazy image takes its pyramid from the source")
{
    std::vector<Level> levels {
        Level(3000, 1500, 1.0, 1.0),
        Level(1500, 750, 2.0, 2.0),
        Level(750, 375, 4.0, 4.0),
    };
    Image img(std::make_shared<StepPyramidSource>(levels), 3000, 1500, 1);

    REQUIRE(img.getLevelCount() == 3);
    // level 0 dimensions are the image's, whatever else exists
    CHECK(img.getLevel(0).w == 3000);
    CHECK(img.w == 3000);
    CHECK(img.h == 1500);
    CHECK(img.getLevel(2).w == 750);
    CHECK(img.getLevel(2).scale() == 4.0);
    // 750x375 is one chunk wide, 3000x1500 is three
    CHECK(img.getLevel(0).cw() == 3);
    CHECK(img.getLevel(2).cw() == 1);

    SUBCASE("chunks come from the level they were asked for")
    {
        auto fine = img.getChunk(0, 0, 0, 0);
        REQUIRE(bool(fine));
        CHECK(fine->pixels[0] == 0.f);
        auto coarse = img.getChunk(2, 0, 0, 0);
        REQUIRE(bool(coarse));
        CHECK(coarse->pixels[0] == 2.f);
        // the coarse level's only chunk is clipped to its own size
        CHECK(coarse->w == 750);
        CHECK(coarse->h == 375);
    }

    SUBCASE("levels are independent residency grids")
    {
        CHECK(bool(img.getChunk(2, 0, 0, 0)));
        CHECK(bool(img.getChunk(1, 0, 1, 0)));
        CHECK(bool(img.getChunk(3, 0, 0, 0)) == false); // no such level
    }

    SUBCASE("probing answers from the level asked for, without reading anything")
    {
        float v[3] = { -1, -1, -1 };
        // nothing is resident, and a probe must not queue a read
        CHECK(img.getPixelValueAtBands(2500, 1000, BANDS_DEFAULT, v, 2)[0] == false);
        CHECK(img.getPixelValueAtBands(2500, 1000, BANDS_DEFAULT, v, 0)[0] == false);

        // the level-0 pixel 2500,1000 lives at 625,250 of level 2, i.e. in its
        // only chunk, whereas at level 0 it is in chunk (2,0)
        REQUIRE(bool(img.getChunk(2, 0, 0, 0)));
        auto valids = img.getPixelValueAtBands(2500, 1000, BANDS_DEFAULT, v, 2);
        CHECK(valids[0] == true);
        CHECK(v[0] == 2.f); // read at level 2, not at level 0
        CHECK(valids[1] == false); // single band image

        // still nothing resident at level 0, so the fine value is not reported
        CHECK(img.getPixelValueAtBands(2500, 1000, BANDS_DEFAULT, v, 0)[0] == false);
        REQUIRE(bool(img.getChunk(0, 0, 2, 0)));
        CHECK(img.getPixelValueAtBands(2500, 1000, BANDS_DEFAULT, v, 0)[0] == true);
        CHECK(v[0] == 0.f);

        // out of bounds is still out of bounds, in level-0 coordinates
        CHECK(img.getPixelValueAtBands(3000, 0, BANDS_DEFAULT, v, 2)[0] == false);
        // a level that does not exist answers nothing rather than level 0
        CHECK(img.getPixelValueAtBands(2500, 1000, BANDS_DEFAULT, v, 9)[0] == false);
    }
}

TEST_CASE("a source without a pyramid still gets level 0")
{
    std::vector<Level> levels; // nothing described
    Image img(std::make_shared<StepPyramidSource>(levels), 100, 50, 1);
    REQUIRE(img.getLevelCount() == 1);
    CHECK(img.getLevel(0).w == 100);
    CHECK(img.getLevel(0).scale() == 1.0);
}

TEST_CASE("a source describing a bogus level 0 is ignored")
{
    // level 0 must be full resolution; everything in vpv works in its pixels
    std::vector<Level> levels { Level(50, 25, 2.0, 2.0) };
    Image img(std::make_shared<StepPyramidSource>(levels), 100, 50, 1);
    REQUIRE(img.getLevelCount() == 1);
    CHECK(img.getLevel(0).w == 100);
    CHECK(img.getLevel(0).scale() == 1.0);
}

TEST_CASE("ImageStats starts out knowing nothing")
{
    ImageStats stats;
    CHECK(stats.generation == 0);
    CHECK(stats.quantiles.empty());
}

namespace {

// Records which chunks were read, and gives every pixel a value derived from
// the level and from its position, so that a scan is recognisable.
class CountingSource : public ChunkSource {
    std::vector<Level> levels;

public:
    std::vector<std::pair<size_t, BandIndex>> reads; // (level, band)
    size_t bandcount;

    CountingSource(const std::vector<Level>& levels, size_t bandcount)
        : levels(levels)
        , bandcount(bandcount)
    {
    }

    std::shared_ptr<Chunk> fetch(size_t level, BandIndex band, size_t cx, size_t cy) override
    {
        if (level >= levels.size() || band >= bandcount)
            return nullptr;
        const Level& lv = levels[level];
        if (cx >= lv.cw() || cy >= lv.ch())
            return nullptr;
        reads.emplace_back(level, band);
        auto chunk = std::make_shared<Chunk>(lv.chunkWidth(cx), lv.chunkHeight(cy));
        for (size_t y = 0; y < chunk->h; y++) {
            for (size_t x = 0; x < chunk->w; x++) {
                // band 0 spans [0,w), band 1 is negative: a band-dependent range
                float v = (float)(cx * CHUNK_SIZE + x);
                chunk->pixels[y * chunk->w + x] = band == 1 ? -v : v;
            }
        }
        return chunk;
    }

    std::vector<Level> describeLevels() const override { return levels; }
};

}

TEST_CASE("statistics of a lazy image come from the coarsest level")
{
    std::vector<Level> levels {
        Level(4000, 2000, 1.0, 1.0),
        Level(1000, 500, 4.0, 4.0),
    };
    auto source = std::make_shared<CountingSource>(levels, 2);
    Image img(source, 4000, 2000, 2);

    std::vector<float> savedsat = gSaturations;
    gSaturations = { 0.1f };

    // nothing is known before the pass has run
    CHECK(img.stats.generation == 0);
    CHECK(img.wantsStats(BANDS_DEFAULT) == true);

    img.computeStats(BANDS_DEFAULT);
    gSaturations = savedsat;

    CHECK(img.stats.generation == 1);
    CHECK(img.stats.level == 1); // the coarsest one
    CHECK(img.stats.approximate == true);
    CHECK(img.stats.bands == BANDS_DEFAULT);
    // band 0 goes up to 999 over the coarse level, band 1 down to -999; band 2
    // does not exist and must not have been asked for
    CHECK(img.stats.min == -999.f);
    CHECK(img.stats.max == 999.f);

    SUBCASE("the saturation cuts were computed in the same pass")
    {
        // values are 0..999 and -999..0, so a 10% cut lands well inside
        float low = 0, high = 0;
        REQUIRE(img.getQuantiles(0.1f, BANDS_DEFAULT, low, high));
        CHECK(low > -999.f);
        CHECK(high < 999.f);
        CHECK(low < high);
        // a quantile nobody configured, and a selection that was not scanned
        CHECK(img.getQuantiles(0.42f, BANDS_DEFAULT, low, high) == false);
        CHECK(img.getQuantiles(0.1f, BandIndices { 1, 1, 1 }, low, high) == false);
    }

    SUBCASE("only the requested bands, only the coarsest level, were read")
    {
        for (const auto& r : source->reads) {
            CHECK(r.first == 1);
            CHECK(r.second < 2);
        }
        CHECK(!source->reads.empty());
    }

    SUBCASE("the pass is not queued again for the same bands")
    {
        img.markStatsRequested(BANDS_DEFAULT);
        CHECK(img.wantsStats(BANDS_DEFAULT) == false);
        // ... but it is for another selection: only the displayed bands are
        // scanned, so the numbers do not apply to a different one
        CHECK(img.wantsStats(BandIndices { 1, 1, 1 }) == true);
    }

    SUBCASE("re-scanning another band gives that band's range")
    {
        img.computeStats(BandIndices { 1, 1, 1 });
        CHECK(img.stats.generation == 2);
        CHECK(img.stats.max == 0.f); // band 1 is all negative
        CHECK(img.stats.min == -999.f);
    }
}

TEST_CASE("an eager image is scanned per band selection too")
{
    // band 0 in [0,4), band 1 in [100,104)
    const size_t w = 2, h = 2, c = 2;
    float* pixels = (float*)malloc(sizeof(float) * w * h * c);
    for (size_t i = 0; i < w * h; i++) {
        pixels[i * c + 0] = (float)i;
        pixels[i * c + 1] = 100.f + (float)i;
    }
    Image img(pixels, w, h, c);

    // no scan at construction any more: one code path for eager and lazy
    CHECK(img.stats.generation == 0);
    CHECK(img.wantsStats(BANDS_DEFAULT) == true);

    SUBCASE("the default selection covers both bands")
    {
        img.computeStats(BANDS_DEFAULT);
        CHECK(img.stats.min == 0.f);
        CHECK(img.stats.max == 103.f);
        CHECK(img.stats.level == 0);
        // every pixel of every band, at full resolution
        CHECK(img.stats.approximate == false);
        // quantiles stay exact and on demand for an eager image
        CHECK(img.stats.quantiles.empty());
    }

    SUBCASE("selecting one band gives that band's range")
    {
        img.computeStats(BandIndices { 0, 0, 0 });
        CHECK(img.stats.min == 0.f);
        CHECK(img.stats.max == 3.f);
        // only one of the two bands was looked at
        CHECK(img.stats.approximate == true);
    }
}

TEST_CASE("an evicted chunk is simply read again")
{
    std::vector<Level> levels { Level(2 * CHUNK_SIZE, CHUNK_SIZE, 1.0, 1.0) };
    auto source = std::make_shared<CountingSource>(levels, 1);
    Image img(source, 2 * CHUNK_SIZE, CHUNK_SIZE, 1);

    REQUIRE(bool(img.getChunk(0, 0, 0, 0)));
    CHECK(source->reads.size() == 1);
    // resident: the grid holds a weak reference and the cache the strong one
    REQUIRE(bool(img.getChunk(0, 0, 0, 0)));
    CHECK(source->reads.size() == 1);

    // the cache dropping it is all it takes; the grid slot simply goes empty
    ChunkCache::flush();
    REQUIRE(bool(img.getChunk(0, 0, 0, 0)));
    CHECK(source->reads.size() == 2);

    SUBCASE("a chunk somebody is still holding survives eviction")
    {
        std::shared_ptr<Chunk> held = img.getChunk(0, 0, 1, 0);
        REQUIRE(bool(held));
        std::weak_ptr<Chunk> weak = held;
        ChunkCache::flush();
        CHECK(bool(weak.lock())); // still ours
        // and it is still what the image hands out, so nobody sees two versions
        CHECK(img.getChunk(0, 0, 1, 0).get() == held.get());
    }

    SUBCASE("an image that dies takes its chunks with it")
    {
        std::weak_ptr<Chunk> weak;
        {
            Image other(std::make_shared<CountingSource>(levels, 1), 2 * CHUNK_SIZE, CHUNK_SIZE, 1);
            weak = other.getChunk(0, 0, 0, 0);
            CHECK(bool(weak.lock()));
        }
        CHECK(!weak.lock());
    }

    ChunkCache::flush();
}

TEST_CASE("a lazy image reports nothing to the cache's budget")
{
    // its chunks are the chunk cache's business, and counting them here would
    // count them twice
    std::vector<Level> levels { Level(CHUNK_SIZE, CHUNK_SIZE, 1.0, 1.0) };
    auto source = std::make_shared<CountingSource>(levels, 1);
    Image lazy(source, CHUNK_SIZE, CHUNK_SIZE, 1);
    REQUIRE(bool(lazy.getChunk(0, 0, 0, 0)));
    CHECK(lazy.memoryFootprint() == 0);

    float* pixels = (float*)calloc(4, sizeof(float));
    Image eager(pixels, 2, 2, 1);
    CHECK(eager.memoryFootprint() == 4 * sizeof(float));

    ChunkCache::flush();
}

TEST_CASE("a coarsest level bigger than the budget is subsampled")
{
    // 4 x 2 chunks at the only level available, i.e. no overviews at all
    std::vector<Level> levels { Level(4 * CHUNK_SIZE, 2 * CHUNK_SIZE, 1.0, 1.0) };
    auto source = std::make_shared<CountingSource>(levels, 1);
    Image img(source, 4 * CHUNK_SIZE, 2 * CHUNK_SIZE, 1);

    size_t saved = gStatsMaxPixels;
    gStatsMaxPixels = CHUNK_SIZE * CHUNK_SIZE; // one chunk's worth
    img.computeStats(BandIndices { 0, 0, 0 });
    gStatsMaxPixels = saved;

    CHECK(img.stats.generation == 1);
    // 8 chunks, budget of 1: stride 3, so (0,0) and (3,0) only
    CHECK(source->reads.size() == 2);
    CHECK(img.stats.approximate == true);
}
