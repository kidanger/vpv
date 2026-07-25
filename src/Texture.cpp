#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include <GL/gl3w.h>

#include "Image.hpp"
#include "OpenGLDebug.hpp"
#include "Texture.hpp"
#include "globals.hpp"

// Tiles always hold three interleaved bands, whatever the image's channel
// count. A missing band reads as zero, which is what sampling a GL_RED or
// GL_RG texture used to give, so this is behaviour-preserving. It costs VRAM
// for single-band images, which is more than paid back by no longer allocating
// the whole tile grid up front.
#define TILE_FORMAT GL_RGB
#define TILE_INTERNAL_FORMAT GL_RGB32F
#define TILE_CHANNELS 3

// Every tile of every Texture lives here, so that GPU_CACHE_LIMIT bounds the
// whole application: several windows looking at the same big image used to get
// one budget each. Keyed by owner first, so a Texture's tiles are contiguous.
namespace {
using TileKey = std::tuple<const void*, size_t, size_t, size_t>; // owner, level, cx, cy

std::map<TileKey, TextureTile> gTiles;
uint64_t textureClock = 0;
uint64_t frameStart = 0; // tiles used after this point are in use this frame
size_t tileBytes = 0;

size_t tileSize(const TextureTile& t)
{
    return t.w * t.h * TILE_CHANNELS * sizeof(float);
}
}

static void initTile(TextureTile t)
{
    glBindTexture(GL_TEXTURE_2D, t.id);
    GLDEBUG();
    glTexImage2D(GL_TEXTURE_2D, 0, TILE_INTERNAL_FORMAT, t.w, t.h, 0, TILE_FORMAT, GL_FLOAT, nullptr);
    GLDEBUG();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLDEBUG();
    switch (gDownsamplingQuality) {
    case 0:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        break;
    case 1:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        break;
    case 2:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        break;
    case 3:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        break;
    }
    GLDEBUG();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLDEBUG();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLDEBUG();

    glBindTexture(GL_TEXTURE_2D, 0);
    GLDEBUG();
}

static TextureTile takeTile(size_t w, size_t h)
{
    TextureTile tile;
    glGenTextures(1, &tile.id);
    GLDEBUG();
    tile.w = w;
    tile.h = h;
    initTile(tile);
    return tile;
}

// Deletes the GL object, rather than pooling it for reuse. A pooled texture
// keeps its storage allocated until it is re-specified, so recycling would have
// made GPU_CACHE_LIMIT a fiction: the bytes would leave our count and stay in
// VRAM. Re-specifying a 12MB texture costs the same whether the name is fresh or
// recycled, so the pool only ever saved a glGenTextures call.
static void giveTile(TextureTile t)
{
    glDeleteTextures(1, &t.id);
    GLDEBUG();
}

void Texture::clear()
{
    for (auto it = gTiles.begin(); it != gTiles.end();) {
        if (std::get<0>(it->first) == this) {
            tileBytes -= tileSize(it->second);
            giveTile(it->second);
            it = gTiles.erase(it);
        } else {
            it++;
        }
    }
}

Texture::~Texture()
{
    clear();
}

void Texture::beginFrame()
{
    frameStart = textureClock;
}

size_t Texture::bytes()
{
    return tileBytes;
}

const TextureTile* Texture::getTile(size_t level, size_t cx, size_t cy)
{
    auto it = gTiles.find(TileKey { this, level, cx, cy });
    if (it == gTiles.end())
        return nullptr;
    // Drawn this frame, whichever level it belongs to: a coarse tile used as a
    // fallback must not be the one eviction picks.
    it->second.lastUsed = ++textureClock;
    return &it->second;
}

// Frees tiles, anywhere in the application, until VRAM is back under
// GPU_CACHE_LIMIT. A tile that was drawn during the current frame is never
// picked, and neither is one this update() is about to need: evicting either
// would have it re-uploaded immediately. If that leaves nothing to free we go
// over budget for the frame rather than thrash.
static void evictTiles(const std::set<TileKey>& keep)
{
    size_t limit = gGpuCacheLimitMB * 1000000;
    if (!limit)
        return;

    while (tileBytes > limit) {
        auto oldest = gTiles.end();
        for (auto it = gTiles.begin(); it != gTiles.end(); it++) {
            if (it->second.lastUsed > frameStart)
                continue; // in use this frame
            if (keep.count(it->first))
                continue;
            if (oldest == gTiles.end() || it->second.lastUsed < oldest->second.lastUsed)
                oldest = it;
        }
        if (oldest == gTiles.end())
            break;
        tileBytes -= tileSize(oldest->second);
        giveTile(oldest->second);
        gTiles.erase(oldest);
    }
}

void Texture::update(const std::shared_ptr<Image>& image, size_t level, BandIndices bands,
    const std::vector<std::pair<size_t, size_t>>& chunks)
{
    if (!image || level >= image->getLevelCount())
        return;

    // A level change deliberately keeps the tiles: the level we are leaving is
    // what fills in for the one we are arriving at until its chunks are read.
    if (image != currentImage || bands != currentBands) {
        clear();
        currentImage = image;
        currentBands = bands;
    }

    const Level& lv = image->getLevel(level);

    // Chunks we are about to draw: protected from eviction for the whole call,
    // not just once we get to them, otherwise a big upload run would evict its
    // own earlier tiles.
    std::set<TileKey> keep;
    for (const auto& coord : chunks) {
        keep.insert(TileKey { this, level, coord.first, coord.second });
    }

    // NOTE: still slow: one synchronous interleave + upload per tile on the
    // render thread. PBOs and a threaded interleave are still wanted.
    static std::vector<float> interleaved(CHUNK_SIZE * CHUNK_SIZE * TILE_CHANNELS);

    for (const auto& coord : chunks) {
        size_t cx = coord.first;
        size_t cy = coord.second;
        if (cx >= lv.cw() || cy >= lv.ch())
            continue;

        TileKey key { this, level, cx, cy };
        auto it = gTiles.find(key);
        if (it != gTiles.end()) {
            it->second.lastUsed = ++textureClock;
            continue;
        }

        std::shared_ptr<Chunk> src[TILE_CHANNELS];
        size_t tw = lv.chunkWidth(cx);
        size_t th = lv.chunkHeight(cy);
        // A tile is uploaded once and then never revisited, so it must not be
        // built out of a partially arrived set of bands: wait until every band
        // that exists is resident. Bands that do not exist upload as zeros,
        // which is what sampling a GL_RED/GL_RG texture used to give.
        size_t existing = 0;
        bool complete = true;
        for (int b = 0; b < TILE_CHANNELS; b++) {
            if (bands[b] >= image->c)
                continue;
            existing++;
            src[b] = image->getChunk(level, bands[b], cx, cy);
            if (!src[b]) {
                complete = false;
                continue;
            }
            if (src[b]->w != tw || src[b]->h != th) {
                assert(0 && "chunk size does not match its level");
                src[b] = nullptr;
                complete = false;
            }
        }
        if (existing && !complete) {
            // the caller draws a placeholder and we will be asked again next frame
            continue;
        }

        for (int b = 0; b < TILE_CHANNELS; b++) {
            if (!src[b]) {
                for (size_t i = 0; i < tw * th; i++) {
                    interleaved[i * TILE_CHANNELS + b] = 0.f;
                }
                continue;
            }
            const Chunk& chunk = *src[b];
            for (size_t y = 0; y < th; y++) {
                const float* in = &chunk.pixels[y * chunk.w];
                float* out = &interleaved[y * tw * TILE_CHANNELS + b];
                for (size_t x = 0; x < tw; x++) {
                    out[x * TILE_CHANNELS] = in[x];
                }
            }
        }

        TextureTile tile = takeTile(tw, th);
        tile.x = cx * CHUNK_SIZE;
        tile.y = cy * CHUNK_SIZE;
        tile.lastUsed = ++textureClock;

        glBindTexture(GL_TEXTURE_2D, tile.id);
        GLDEBUG();
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        GLDEBUG();
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw, th, TILE_FORMAT, GL_FLOAT, interleaved.data());
        GLDEBUG();

        if (gDownsamplingQuality >= 2) {
            glGenerateMipmap(GL_TEXTURE_2D);
            GLDEBUG();
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        GLDEBUG();

        gTiles[key] = tile;
        tileBytes += tileSize(tile);
        evictTiles(keep);
    }
}
