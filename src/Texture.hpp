#pragma once

#include <memory>
#include <vector>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "Image.hpp"

struct TextureTile {
    unsigned id;
    int x, y;
    size_t w, h;
    unsigned format;
};

struct Texture {
    std::vector<TextureTile> tiles;
    ImVec2 size;
    unsigned format = -1;

    ~Texture();

    void upload(const Image& img, ImRect area, BandIndices bandidx = { 0, 1, 2 });
    ImVec2 getSize() const { return size; }

private:
    void create(size_t w, size_t h, unsigned format);
};
