#pragma once

#include <vector>
#include <memory>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

struct Image;

struct Tile {
    unsigned id;
    int x, y;
    size_t w, h;
    unsigned format;
};

struct Texture {
    std::vector<Tile> tiles;
    ImVec2 size;
    unsigned format = -1;

    ~Texture();

    void create(size_t w, size_t h, unsigned format);
    void upload(const std::shared_ptr<Image>& img, ImRect area);
    ImVec2 getSize() { return size; }
};

