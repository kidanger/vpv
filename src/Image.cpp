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

Image::Image(float* pixels, size_t w, size_t h, size_t format)
    : pixels(pixels), w(w), h(h), format(format), lastUsed(0)
{
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::min();
    for (size_t i = 0; i < w*h*format; i++) {
        float v = pixels[i];
        min = std::min(min, v);
        max = std::max(max, v);
    }
    if (!std::isfinite(min) || !std::isfinite(max)) {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::min();
        for (size_t i = 0; i < w*h*format; i++) {
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

    const float* data = (float*) pixels + (w * y + x)*format;
    const float* end = (float*) pixels + (w * h)*format;
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
    histograms.resize(format);

    // TODO: oversample the histogram and resample on the fly

    const int nbins = 256;
    for (size_t d = 0; d < format; d++) {
        auto& histogram = histograms[d];
        histogram.clear();
        histogram.resize(nbins);

        float f = nbins / (max - min);
        for (size_t i = 0; i < w*h; i++) {
            int bin = (pixels[i*format+d] - min) * f;
            if (bin >= 0 && bin < nbins) {
                histogram[bin]++;
            }
        }
    }
    histmin = min;
    histmax = max;
}

#if 0
    watcher_add_file(filename, [](const std::string& filename) {
        lock.lock();
        if (cache.find(filename) != cache.end()) {
            Image* img = cache[filename];
            for (auto seq : gSequences) {
                seq->forgetImage();
            }
            delete img;
            cache.erase(filename);
            cacheSize -= img->w * img->h * img->format * sizeof(float);
        }
        lock.unlock();
        printf("'%s' modified on disk, cache invalidated\n", filename.c_str());
    });
#endif

