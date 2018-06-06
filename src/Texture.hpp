#pragma once

#include <vector>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

class Image;
struct Texture {
    std::vector<int> ids;
    unsigned int id = -1;
    ImVec2 size;
    unsigned int format;

    ~Texture();

    void create(int w, int h, unsigned int format);
    void upload(const Image* img, ImRect area);
    ImVec2 getSize() { return size; }
};

