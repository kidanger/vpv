#include <algorithm>
#include <cstdlib>

#include "ChunkCache.hpp"
#include "EditChunkSource.hpp"
#include "Image.hpp"

EditChunkSource::EditChunkSource(EditType edittype, std::string prog,
    std::vector<std::shared_ptr<Image>> inputs)
    : edittype(edittype)
    , prog(std::move(prog))
    , inputs(std::move(inputs))
{
    // the geometry of the result is the geometry of input 0, as in the
    // whole-image path (which hands plambda w[0]/h[0])
    const Image& ref = *this->inputs[0];
    for (size_t l = 0; l < ref.getLevelCount(); l++) {
        const Level& lv = ref.getLevel(l);
        levels.emplace_back(lv.w, lv.h, lv.scaleX, lv.scaleY);
    }
}

std::shared_ptr<Image> EditChunkSource::assembleInput(size_t i, size_t level,
    size_t x0, size_t y0, size_t tw, size_t th) const
{
    const Image& in = *inputs[i];
    // an input with fewer levels (an eager one has exactly one) is read at the
    // closest level it has; the result is then level-dependent, which is
    // unavoidable and documented
    size_t li = std::min(level, in.getLevelCount() - 1);
    const Level& ilv = in.getLevel(li);
    size_t ci = in.c;

    float* pixels = (float*)malloc(sizeof(float) * tw * th * ci);
    if (!pixels)
        return nullptr;

    for (size_t b = 0; b < ci; b++) {
        std::shared_ptr<Chunk> chunk;
        size_t haveCx = (size_t)-1, haveCy = (size_t)-1;

        for (size_t y = 0; y < th; y++) {
            // out of range reads are clamped, which is what plambda's own
            // default boundary policy does at the border of a whole image
            size_t sy = std::min(y0 + y, ilv.h - 1);
            for (size_t x = 0; x < tw; x++) {
                size_t sx = std::min(x0 + x, ilv.w - 1);
                size_t cxs = sx / CHUNK_SIZE;
                size_t cys = sy / CHUNK_SIZE;
                if (!chunk || cxs != haveCx || cys != haveCy) {
                    chunk = in.getChunkBlocking(li, b, cxs, cys);
                    haveCx = cxs;
                    haveCy = cys;
                    if (!chunk) {
                        // a permanent failure (an IO error, a band that does not
                        // exist): fail the whole tile rather than cache zeros
                        free(pixels);
                        return nullptr;
                    }
                }
                pixels[(y * tw + x) * ci + b]
                    = chunk->pixels[(sy - cys * CHUNK_SIZE) * chunk->w + (sx - cxs * CHUNK_SIZE)];
            }
        }
    }

    return std::make_shared<Image>(pixels, tw, th, ci);
}

std::vector<std::shared_ptr<Chunk>> EditChunkSource::evalTile(size_t level, size_t cx, size_t cy,
    std::string& error)
{
    if (level >= levels.size()) {
        error = "no such level";
        return {};
    }
    const Level& lv = levels[level];
    if (cx >= lv.cw() || cy >= lv.ch()) {
        error = "tile outside the image";
        return {};
    }

    size_t x0 = cx * CHUNK_SIZE;
    size_t y0 = cy * CHUNK_SIZE;
    size_t tw = lv.chunkWidth(cx);
    size_t th = lv.chunkHeight(cy);

    std::vector<std::shared_ptr<Image>> tiles;
    for (size_t i = 0; i < inputs.size(); i++) {
        std::shared_ptr<Image> tile = assembleInput(i, level, x0, y0, tw, th);
        if (!tile) {
            error = "cannot read the input tile";
            return {};
        }
        tiles.push_back(tile);
    }

    std::shared_ptr<Image> out = edit_images(edittype, prog, tiles, error);
    if (!out)
        return {};

    if (out->w != tw || out->h != th) {
        error = "the program returned a tile of a different size";
        return {};
    }
    if (out->c == 0 || (outc != 0 && out->c != outc)) {
        // a ragged image is worse than an error: everything downstream (the band
        // selection, the statistics, the textures) assumes one channel count
        error = "the program returned a different number of channels for different tiles";
        return {};
    }
    outc = out->c;

    std::vector<std::shared_ptr<Chunk>> chunks;
    for (size_t b = 0; b < outc; b++) {
        auto chunk = std::make_shared<Chunk>(tw, th);
        for (size_t y = 0; y < th; y++)
            for (size_t x = 0; x < tw; x++)
                chunk->pixels[y * tw + x] = out->pixels[(y * tw + x) * outc + b];
        chunks.push_back(chunk);
    }
    return chunks;
}

bool EditChunkSource::probe(std::string& error)
{
    // the coarsest level is the cheapest place to discover the channel count
    size_t level = levels.size() - 1;
    std::vector<std::shared_ptr<Chunk>> chunks = evalTile(level, 0, 0, error);
    if (chunks.empty()) {
        if (error.empty())
            error = "cannot evaluate the first tile";
        return false;
    }

    for (const auto& chunk : chunks)
        ChunkCache::retain(chunk, nullptr);

    std::lock_guard<std::mutex> lock(memoMutex);
    memoValid = true;
    memoLevel = level;
    memoCx = memoCy = 0;
    memoBands.assign(chunks.begin(), chunks.end());
    return true;
}

std::shared_ptr<Chunk> EditChunkSource::read(size_t level, BandIndex band, size_t cx, size_t cy,
    std::string& error)
{
    {
        std::lock_guard<std::mutex> lock(memoMutex);
        if (memoValid && memoLevel == level && memoCx == cx && memoCy == cy) {
            if (band >= memoBands.size()) {
                error = "no such band";
                return nullptr;
            }
            if (std::shared_ptr<Chunk> chunk = memoBands[band].lock())
                return chunk;
        }
    }

    std::vector<std::shared_ptr<Chunk>> chunks = evalTile(level, cx, cy, error);
    if (chunks.empty())
        return nullptr;

    // every band of the tile is now accounted for and evictable, like any other
    // chunk; the owner is unknown here, exactly as for a speculative read
    for (const auto& chunk : chunks)
        ChunkCache::retain(chunk, nullptr);

    {
        std::lock_guard<std::mutex> lock(memoMutex);
        memoValid = true;
        memoLevel = level;
        memoCx = cx;
        memoCy = cy;
        memoBands.assign(chunks.begin(), chunks.end());
    }

    if (band >= chunks.size()) {
        error = "no such band";
        return nullptr;
    }
    return chunks[band];
}

#include "doctest.h"

#ifdef USE_PLAMBDA
TEST_SUITE("EditChunkSource")
{
    static std::shared_ptr<Image> ramp(size_t w, size_t h, size_t c)
    {
        float* pixels = (float*)malloc(sizeof(float) * w * h * c);
        for (size_t y = 0; y < h; y++)
            for (size_t x = 0; x < w; x++)
                for (size_t b = 0; b < c; b++)
                    pixels[(y * w + x) * c + b] = (float)(x + 3 * y + 100 * b);
        return std::make_shared<Image>(pixels, w, h, c);
    }

    TEST_CASE("a per pixel program tiles exactly")
    {
        // wider than one chunk, so that several tiles are evaluated
        std::shared_ptr<Image> in = ramp(1500, 1100, 2);

        std::string error;
        std::shared_ptr<Image> whole = edit_images(PLAMBDA, "x 2 *", { in }, error);
        REQUIRE(bool(whole));

        auto source = std::make_shared<EditChunkSource>(PLAMBDA, "x 2 *",
            std::vector<std::shared_ptr<Image>> { in });
        REQUIRE(source->probe(error));
        CHECK(source->bandCount() == 2);

        std::shared_ptr<Image> edited = std::make_shared<Image>(source, in->w, in->h,
            source->bandCount());

        for (size_t b = 0; b < 2; b++) {
            for (size_t cy = 0; cy < 2; cy++) {
                for (size_t cx = 0; cx < 2; cx++) {
                    std::shared_ptr<Chunk> chunk = edited->getChunkBlocking(0, b, cx, cy);
                    REQUIRE(bool(chunk));
                    for (size_t y = 0; y < chunk->h; y++) {
                        for (size_t x = 0; x < chunk->w; x++) {
                            size_t gx = cx * CHUNK_SIZE + x;
                            size_t gy = cy * CHUNK_SIZE + y;
                            CHECK(chunk->pixels[y * chunk->w + x]
                                == whole->pixels[(gy * whole->w + gx) * 2 + b]);
                        }
                    }
                }
            }
        }
    }

    TEST_CASE("the channel count may differ from the input's")
    {
        std::shared_ptr<Image> in = ramp(100, 50, 1);
        auto source = std::make_shared<EditChunkSource>(PLAMBDA, "x x join",
            std::vector<std::shared_ptr<Image>> { in });
        std::string error;
        REQUIRE(source->probe(error));
        CHECK(source->bandCount() == 2);
        CHECK(source->describeLevels().size() == 1);
    }
}
#endif
