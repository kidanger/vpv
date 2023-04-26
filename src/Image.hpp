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
    enum Format {
        U8,
        U16,
        I8,
        I16,
        // everything else is stored as F32
        F32,
    } format;

    ImVec2 size;
    float min;
    float max;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;

    Image(float* pixels, size_t w, size_t h, size_t c);
    Image(void* pixels, size_t w, size_t h, size_t c, Format format);
    ~Image();

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool, 3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

    std::pair<float, float> minmax_in_rect(BandIndices bands, ImVec2 p1, ImVec2 p2) const;
    std::pair<float, float> quantiles(BandIndices bands, float quantile) const;
    std::pair<float, float> quantiles_in_rect(BandIndices bands, float quantile,
        ImVec2 p1, ImVec2 p2) const;
    const float* extract_into_glbuffer(BandIndices bands, ImRect intersect, float* reshapebuffer, size_t tw, size_t th, size_t max_size, bool needsreshape) const;
    float at(size_t p) const
    {
        switch (format) {
        case U8:
            return (float)reinterpret_cast<uint8_t*>(pixels)[p];
        case I8:
            return (float)reinterpret_cast<int8_t*>(pixels)[p];
        case U16:
            return (float)reinterpret_cast<uint16_t*>(pixels)[p];
        case I16:
            return (float)reinterpret_cast<int16_t*>(pixels)[p];
        case F32:
            return reinterpret_cast<float*>(pixels)[p];
        }
        return 0; // cannot happen
    }
    float at(size_t x, size_t y, size_t d) const
    {
        return at(index(x, y, d));
    }
    size_t index(size_t x, size_t y, size_t d) const
    {
        return (y * w + x) * c + d;
    }

private:
    void* pixels;
};
