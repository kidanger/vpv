#pragma once

#include <vector>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

class Image;

struct Tile {
    unsigned int id;
    int x, y;
    int w, h;
};

struct Texture {
    //unsigned int id = -1;
    std::vector<Tile> tiles;
    ImVec2 size;
    unsigned int format;

    ~Texture();

    void create(int w, int h, unsigned int format);
    void upload(const Image* img, ImRect area);
    ImVec2 getSize() { return size; }
};

