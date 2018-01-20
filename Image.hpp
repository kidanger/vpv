#pragma once

#include <string>
#include <unordered_map>
#include <vector>
struct Image {
    int w, h;
    enum Type {
        UINT8,
        FLOAT,
    } type;
    enum Format {
        R=1,
        RG,
        RGB,
        RGBA,
    } format;
    void* pixels;
    float min;
    float max;
    bool is_cached;

    Image(float* pixels, int w, int h, Format format);
    ~Image();

    void getPixelValueAt(int x, int y, float* values, int d) const;
    void computeHistogram(size_t sizeHisto);
    static Image* load(const std::string& filename, bool force_load=true);

    static std::unordered_map<std::string, Image*> cache;
    static void flushCache();

    std::vector< float> histosValues;
};

