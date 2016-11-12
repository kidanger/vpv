#pragma once

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

struct Texture {
    unsigned int id = -1;
    ImVec2 size;
    unsigned int type;
    unsigned int format;

    ~Texture();

    void create(int w, int h, unsigned int type, unsigned int format);
    ImVec2 getSize() { return size; }
};

