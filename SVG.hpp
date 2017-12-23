#pragma once

#include "imgui.h"
#include "nanosvg.h"

void draw_svg();

struct SVG {
    struct NSVGimage* nsvg;
    std::string filename;
    bool valid;

    ~SVG();

    bool load(const std::string& filename);
    void draw(ImVec2 pos, float zoom) const;
};

