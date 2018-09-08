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

Image::Image(float* pixels, size_t w, size_t h, size_t c)
    : pixels(pixels), w(w), h(h), c(c), lastUsed(0)
{
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::min();
    for (size_t i = 0; i < w*h*c; i++) {
        float v = pixels[i];
        min = std::min(min, v);
        max = std::max(max, v);
    }
    if (!std::isfinite(min) || !std::isfinite(max)) {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::min();
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
    if (x < 0 || y < 0 || x >= w || y >= h)
        return;

    const float* data = (float*) pixels + (w * y + x)*c;
    const float* end = (float*) pixels + (w * h)*c;
    for (size_t i = 0; i < d; i++) {
        if (data + i >= end) break;
        values[i] = data[i];
    }
}

void Image::computeHistogram(float min, float max)
{
    if (!histograms.empty() && histmin == min && histmax == max)
        return;

    histograms.clear();
    histograms.resize(c);

    // TODO: oversample the histogram and resample on the fly

    const int nbins = 256;
    for (size_t d = 0; d < c; d++) {
        auto& histogram = histograms[d];
        histogram.clear();
        histogram.resize(nbins);

        // nbins-1 because we want the last bin to end at 'max' and not start at 'max'
        float f = (nbins-1) / (max - min);
        for (size_t i = 0; i < w*h; i++) {
            float bin = (pixels[i*c+d] - min) * f;
            if (bin >= 0 && bin < nbins) {
                histogram[bin]++;
            }
        }
    }
    histmin = min;
    histmax = max;
}

