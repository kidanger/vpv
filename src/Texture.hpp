#pragma once

#include <cstdint>
#include <map>
#include <memory>
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
// the GPU. Recycled GL texture objects are shared between all Textures through
// a global pool (see Texture.cpp).
struct Texture {
    ~Texture();

    // Uploads whichever of the requested chunks are resident and not already on
    // the GPU. Drops everything if the image, the level or the bands changed.
    // 'chunks' are (cx, cy) coordinates in the chunk grid of 'level', and are
    // uploaded in the order given.
    void update(const std::shared_ptr<Image>& image, size_t level, BandIndices bands,
        const std::vector<std::pair<size_t, size_t>>& chunks);

    // nullptr when that chunk is not on the GPU (yet)
    const TextureTile* getTile(size_t cx, size_t cy) const;

    size_t getLevel() const { return currentLevel; }

private:
    std::map<std::pair<size_t, size_t>, TextureTile> tiles;
    std::shared_ptr<Image> currentImage;
    size_t currentLevel = 0;
    BandIndices currentBands = BANDS_DEFAULT;

    void clear();
    void evict(const std::vector<std::pair<size_t, size_t>>& keep);
};
