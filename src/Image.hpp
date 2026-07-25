#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>

#include <imgui.h>

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
#define BANDS_DEFAULT (BandIndices { 0, 1, 2 })

class Histogram;

// Statistics over an image.
//
// 'generation' is bumped every time the values change. Consumers must use it as
// the invalidation key instead of comparing min/max, because with lazy images
// the stats get refined over time (and comparing floats cannot express "the
// same numbers, but now computed over more data").
//
// 'approximate' is set when the stats were not computed over every pixel at
// full resolution (e.g. estimated from a coarse pyramid level).
struct ImageStats {
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();
    uint64_t generation = 0;
    bool approximate = false;

    void set(float min, float max, bool approximate = false)
    {
        this->min = min;
        this->max = max;
        this->approximate = approximate;
        generation++;
    }
};

struct Image {
    std::string ID;
    float* pixels;
    size_t w, h, c;
    ImVec2 size;
    ImageStats stats;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    ~Image();

    // Returns false if the values are not available (out of bounds, or not
    // resident yet); in that case 'values' is left untouched.
    bool getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool, 3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;
};
