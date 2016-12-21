#include <cmath>
#include <cfloat>
#include <string>
#include <cstdlib>
#include <unordered_map>

extern "C" {
#include "iio.h"
}

#include "Image.hpp"

std::unordered_map<std::string, Image*> Image::cache;

Image::~Image()
{
    free(pixels);
}

void Image::getPixelValueAt(int x, int y, float* values, int d)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return;

    if (type == Image::UINT8) {
        const uint8_t* data = (uint8_t*) pixels + (w * y + x)*format;
        const uint8_t* end = (uint8_t*) pixels + (w * h + w)*format;
        for (int i = 0; i < d; i++) {
            if (data + i >= end) break;
            values[i] = data[i];
        }
    } else if (type == Image::FLOAT) {
        const float* data = (float*) pixels + (w * y + x)*format;
        const float* end = (float*) pixels + (w * h + w)*format;
        for (int i = 0; i < d; i++) {
            if (data + i >= end) break;
            values[i] = data[i];
        }
    }
}

Image* Image::load(const std::string& filename, bool force_load)
{
    auto i = cache.find(filename);
    if (i != cache.end()) {
        return i->second;
    }
    if (!force_load) {
        return 0;
    }

    Image* img = new Image;

    int w, h, d;
    float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);

    img->min = FLT_MAX;
    img->max = FLT_MIN;
    for (int i = 0; i < w*h*d; i++) {
        float v = pixels[i];
        img->min = fminf(img->min, v);
        img->max = fmaxf(img->max, v);
    }

    img->w = w;
    img->h = h;
    img->type = Image::FLOAT;
    img->format = (Image::Format) d;
    img->pixels = pixels;
    cache[filename] = img;
    return img;
}

void Image::flushCache()
{
    cache.clear();
}

