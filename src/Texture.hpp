#pragma once

#include <vector>
#include <memory>

#include "optional.hpp"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "Image.hpp"

struct TextureTile {
    unsigned id;
    int x, y;
    size_t w, h;
};

struct Texture {
    std::vector<std::vector<nonstd::optional<TextureTile>>> tiles;
    std::vector<std::pair<size_t, size_t>> visibility;

    ImVec2 size;
    std::shared_ptr<Image> currentImage;
    BandIndices currentBands;

    ~Texture();

    void upload(const std::shared_ptr<Image>& img, ImRect area, BandIndices bandidx={0,1,2});
    ImVec2 getSize() { return size; }
};

