#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>

#include "imgui.h"

struct Image {
    float* pixels;
    size_t w, h;
    ImVec2 size;
    size_t format;
    float min;
    float max;
    uint64_t lastUsed;
    std::vector<std::vector<long>> histograms;
    float histmin, histmax;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t format);
    ~Image();

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;

    void computeHistogram(float min, float max);

};

