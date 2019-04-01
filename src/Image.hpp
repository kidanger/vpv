#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <memory>

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

class Histogram;

struct Image {
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
    bool cutChannels();

};

