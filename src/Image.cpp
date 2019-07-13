#include <cmath>
#include <cfloat>
#include <string>
#include <cstdlib>
#include <unordered_map>
#include <limits>
#include <mutex>

extern "C" {
#include "iio.h"
}

#include "Image.hpp"
#include "watcher.hpp"
#include "globals.hpp"
#include "Sequence.hpp"
#include "events.hpp"
#include "Histogram.hpp"

Image::Image(float* pixels, size_t w, size_t h, size_t c)
    : pixels(pixels), w(w), h(h), c(c), lastUsed(0), histogram(std::make_shared<Histogram>())
{
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < w*h*c; i++) {
        float v = pixels[i];
        min = std::min(min, v);
        max = std::max(max, v);
    }
    if (!std::isfinite(min) || !std::isfinite(max)) {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < w*h*c; i++) {
            float v = pixels[i];
            if (std::isfinite(v)) {
                min = std::min(min, v);
                max = std::max(max, v);
            }
        }
    }
    size = ImVec2(w, h);
}

#include "ImageCache.hpp"
#include "ImageProvider.hpp"
Image::~Image()
{
    LOG("free image");
    free(pixels);
}

void Image::getPixelValueAt(size_t x, size_t y, float* values, size_t d) const
{
    if (x >= w || y >= h)
        return;

    const float* data = (float*) pixels + (w * y + x)*c;
    const float* end = (float*) pixels + (w * h)*c;
    for (size_t i = 0; i < d; i++) {
        if (data + i >= end) break;
        values[i] = data[i];
    }
}

bool Image::cutChannels()
{
    if (c > 4) {
        for (size_t y = 0; y < (size_t)h; y++) {
            for (size_t x = 0; x < (size_t)w; x++) {
                for (size_t l = 0; l < 4; l++) {
                    pixels[(y*h+x)*4+l] = pixels[(y*h+x)*c+l];
                }
            }
        }
        c = 4;
        return true;
    }
    return false;
}

