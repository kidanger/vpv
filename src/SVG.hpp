#pragma once

#include <string>
#include <unordered_map>

#include "imgui.h"
#include "nanosvg.h"

void draw_svg();

struct SVG {
    struct NSVGimage* nsvg;
    std::string filename;
    bool valid;

    SVG(const std::string& filename);
    ~SVG();

    void draw(ImVec2 pos, float zoom) const;

    static SVG* get(const std::string& filename);

    static std::unordered_map<std::string, SVG*> cache;
    static void flushCache();
};

