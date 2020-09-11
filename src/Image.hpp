#pragma once

#include <set>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <map>

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

typedef size_t BandIndex;
typedef std::array<BandIndex,3> BandIndices;
#define BANDS_DEFAULT (BandIndices{0,1,2})

class Histogram;

struct AOIRequest {
    std::vector<BandIndex> bands;
    size_t ox = 0;
    size_t oy = 0;
    size_t w = 0;
    size_t h = 0;
    // scale, maybe one day
};

struct Band {
    std::vector<float> pixels;
    size_t ox, oy;
    size_t w, h;

    float min, max;
    bool statsAreApproximate;

    bool isLoaded() {
        return pixels.empty();
    }
};

struct Image {
    std::string ID;
    float* pixels;
    size_t w, h;
    size_t c;
    ImVec2 size;
    std::map<BandIndex, Band> bands;
    float min;
    float max;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    Image(std::map<BandIndex,Band>&& pixels, size_t w, size_t h, size_t c);
    ~Image();

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool,3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;
};

