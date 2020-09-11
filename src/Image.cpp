#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>

extern "C" {
#include "iio.h"
}

#include "Image.hpp"
#include "Histogram.hpp"

Image::Image(float* pixels, size_t w, size_t h, size_t c)
    : pixels(pixels), w(w), h(h), c(c), lastUsed(0), histogram(std::make_shared<Histogram>())
{
    static int id = 0;
    id++;
    ID = "Image " + std::to_string(id);

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

Image::Image(std::map<BandIndex,Band>&& bands, size_t w, size_t h, size_t c)
    : pixels(nullptr), bands(std::move(bands)), w(w), h(h), c(c), lastUsed(0), histogram(std::make_shared<Histogram>())
{
    static int id = 0;
    id++;
    ID = "Image " + std::to_string(id);

    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::lowest();
    for (auto& it : this->bands) {
        min = std::min(min, it.second.min);
        max = std::max(max, it.second.max);
    }
    size = ImVec2(w, h);
}

#include "ImageCache.hpp"
#include "ImageProvider.hpp"
Image::~Image()
{
    LOG("free image");
    if (pixels) {
        free(pixels);
    }
}

void Image::getPixelValueAt(size_t x, size_t y, float* values, BandIndex d) const
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

std::array<bool,3> Image::getPixelValueAtBands(size_t x, size_t y, BandIndices bandsidx, float* values) const
{
    std::array<bool,3> valids {false,false,false};
    if (x >= w || y >= h)
        return valids;

    for (size_t i = 0; i < 3; i++) {
        if (bandsidx[i] >= c) continue;
        auto it = bands.find(bandsidx[i]);
        if (it != bands.end()) {
            const Band* b = &it->second;
            if (x < b->ox || y < b->oy || x >= b->ox+b->w || y >= b->oy+b->h)
                continue;
            values[i] = b->pixels[(y-b->oy)*b->w+x-b->ox];
            valids[i] = true;
        }
    }
    return valids;
}

