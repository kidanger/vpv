#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "imgui.h"

struct Image {
    float* pixels;
    int w, h;
    ImVec2 size;
    enum Format {
        R=1,
        RG,
        RGB,
        RGBA,
    } format;
    float min;
    float max;
    bool is_cached;
    uint64_t lastUsed;
    std::vector<std::vector<long>> histograms;
    float histmin, histmax;

    Image(float* pixels, int w, int h, Format format);
    ~Image();

    void getPixelValueAt(int x, int y, float* values, int d) const;

    void computeHistogram(float min, float max);

    //static Image* load(const std::string& filename, bool force_load=true);

    static std::unordered_map<std::string, Image*> cache;
    static void flushCache();
};

