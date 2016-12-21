#pragma once

#include <string>
#include <unordered_map>

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

    ~Image();

    void getPixelValueAt(int x, int y, float* values, int d);

    static Image* load(const std::string& filename, bool force_load=true);

    static std::unordered_map<std::string, Image*> cache;
    static void flushCache();
};

