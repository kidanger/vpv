#pragma once

#include <set>
#include <memory>
#include <string>
#include <array>

#include "imgui.h"

#if 0
struct ImageTile {
    unsigned id;
    int x, y;
    size_t w, h, c;
    size_t scale;
    std::vector<float> pixels;
};
#endif

using BandIndices = std::array<size_t, 3>;
#define BANDS_DEFAULT (BandIndices{0,1,2})

class Histogram;

struct Image {
    std::string ID;
    float* pixels;
    size_t w, h, c;
    ImVec2 size;
    float min;
    float max;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    ~Image();

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool,3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

};

