#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>

#include "imgui.h"

struct ImageTile {
    unsigned id;
    int x, y;
    size_t w, h, c;
    size_t scale;
    std::vector<float> pixels;
};

struct Image {
    std::vector<ImageTile> tiles;
    float* pixels;
    size_t w, h, c;
    ImVec2 size;
    float min;
    float max;
    uint64_t lastUsed;
    std::vector<std::vector<long>> histograms;
    float histmin, histmax;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    ~Image();

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;

    void computeHistogram(float min, float max);

};

