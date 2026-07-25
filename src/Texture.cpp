#include <cassert>
#include <cstring>
#include <list>
#include <memory>
#include <set>

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

// Upper bound on how many tiles a single Texture keeps on the GPU. At
// 1024x1024xRGB32F a tile is 12MB, so this is a ~768MB ceiling. Proper
// budgeting across all images comes with the chunk cache (step 6).
#define TEXTURE_TILE_BUDGET 64

static std::list<TextureTile> tileCache;
static uint64_t textureClock = 0;

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
    for (auto it = tileCache.begin(); it != tileCache.end(); it++) {
        if (it->w == w && it->h == h) {
            TextureTile t = *it;
            tileCache.erase(it);
            return t;
        }
    }

    TextureTile tile;
    if (!tileCache.empty()) {
        tile = tileCache.back();
        tileCache.pop_back();
    } else {
        glGenTextures(1, &tile.id);
        GLDEBUG();
    }
    tile.w = w;
    tile.h = h;
    initTile(tile);
    return tile;
}

static void giveTile(TextureTile t)
{
    tileCache.push_back(t);
}

void Texture::clear()
{
    for (const auto& it : tiles) {
        giveTile(it.second);
    }
    tiles.clear();
}

Texture::~Texture()
{
    clear();
}

const TextureTile* Texture::getTile(size_t cx, size_t cy) const
{
    auto it = tiles.find({ cx, cy });
    if (it == tiles.end())
        return nullptr;
    return &it->second;
}

void Texture::evict(const std::vector<std::pair<size_t, size_t>>& keep)
{
    if (tiles.size() <= TEXTURE_TILE_BUDGET)
        return;

    std::set<std::pair<size_t, size_t>> protected_(keep.begin(), keep.end());
    while (tiles.size() > TEXTURE_TILE_BUDGET) {
        auto oldest = tiles.end();
        for (auto it = tiles.begin(); it != tiles.end(); it++) {
            if (protected_.count(it->first))
                continue;
            if (oldest == tiles.end() || it->second.lastUsed < oldest->second.lastUsed)
                oldest = it;
        }
        if (oldest == tiles.end())
            break; // everything left is in use this frame
        giveTile(oldest->second);
        tiles.erase(oldest);
    }
}

void Texture::update(const std::shared_ptr<Image>& image, size_t level, BandIndices bands,
    const std::vector<std::pair<size_t, size_t>>& chunks)
{
    if (!image || level >= image->getLevelCount())
        return;

    if (image != currentImage || level != currentLevel || bands != currentBands) {
        clear();
        currentImage = image;
        currentLevel = level;
        currentBands = bands;
    }

    const Level& lv = image->getLevel(level);

    // NOTE: still slow: one synchronous interleave + upload per tile on the
    // render thread. PBOs and a threaded interleave belong in step 3.
    static std::vector<float> interleaved(CHUNK_SIZE * CHUNK_SIZE * TILE_CHANNELS);

    for (const auto& coord : chunks) {
        size_t cx = coord.first;
        size_t cy = coord.second;
        if (cx >= lv.cw() || cy >= lv.ch())
            continue;

        auto it = tiles.find(coord);
        if (it != tiles.end()) {
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

        tiles[coord] = tile;
    }

    evict(chunks);
}
