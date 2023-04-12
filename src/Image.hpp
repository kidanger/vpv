#pragma once

#include <array>
#include <memory>
#include <set>
#include <string>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

using BandIndices = std::array<size_t, 3>;
#define BANDS_DEFAULT (BandIndices { 0, 1, 2 })

class Histogram;

struct Image {
    std::string ID;
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
    std::array<bool, 3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

    std::pair<float, float> minmax_in_rect(BandIndices bands, ImVec2 p1, ImVec2 p2) const;
    std::pair<float, float> quantiles(BandIndices bands, float quantile) const;
    std::pair<float, float> quantiles_in_rect(BandIndices bands, float quantile,
        ImVec2 p1, ImVec2 p2) const;
    const float* extract_into_glbuffer(BandIndices bands, ImRect intersect, float* reshapebuffer, size_t tw, size_t th, size_t max_size, bool needsreshape) const;
    float at(size_t x, size_t y, size_t d) const
    {
        return pixels[(y * w + x) * c + d];
    }

private:
    float* pixels;
};
