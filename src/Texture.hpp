#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "Image.hpp"

struct TextureTile {
    unsigned id = 0;
    size_t w = 0, h = 0;
    // position of the tile, in pixels of the level it was uploaded from
    size_t x = 0, y = 0;
    uint64_t lastUsed = 0;
};

// A sparse set of GL textures, one per chunk of one level of one image.
//
// Tiles are allocated on demand: only the chunks that are asked for exist on
// the GPU. Tiles of several levels coexist, because a coarse tile is what gets
// drawn while a finer one is still being read. Recycled GL texture objects are
// shared between all Textures through a global pool (see Texture.cpp).
struct Texture {
    ~Texture();

    // Uploads whichever of the requested chunks are resident and not already on
    // the GPU. Drops everything if the image or the bands changed; a level
    // change keeps the old tiles, they are the fallback for the new level.
    // 'chunks' are (cx, cy) coordinates in the chunk grid of 'level', and are
    // uploaded in the order given.
    void update(const std::shared_ptr<Image>& image, size_t level, BandIndices bands,
        const std::vector<std::pair<size_t, size_t>>& chunks);

    // nullptr when that chunk is not on the GPU (yet). Marks the tile as used,
    // so that anything drawn this frame survives eviction.
    const TextureTile* getTile(size_t level, size_t cx, size_t cy);

private:
    using TileKey = std::tuple<size_t, size_t, size_t>; // level, cx, cy

    std::map<TileKey, TextureTile> tiles;
    std::shared_ptr<Image> currentImage;
    BandIndices currentBands = BANDS_DEFAULT;

    void clear();
    void evict(size_t level, const std::vector<std::pair<size_t, size_t>>& keep);
};
