#include <cmath>
#include <cfloat>
#include <string>
#include <cstdlib>
#include <unordered_map>
#include <mutex>

extern "C" {
#include "iio.h"
}

#include "Image.hpp"
#include "watcher.hpp"
#include "globals.hpp"
#include "Sequence.hpp"

std::unordered_map<std::string, Image*> Image::cache;

Image::Image(float* pixels, int w, int h, Format format)
    : pixels(pixels), w(w), h(h), format(format), is_cached(false)
{
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::min();
    for (int i = 0; i < w*h*format; i++) {
        float v = pixels[i];
        if (std::isfinite(v)) {
            min = fminf(min, v);
            max = fmaxf(max, v);
        }
    }
}

Image::~Image()
{
    free(pixels);
}

void Image::getPixelValueAt(int x, int y, float* values, int d) const
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return;

    const float* data = (float*) pixels + (w * y + x)*format;
    const float* end = (float*) pixels + (w * h)*format;
    for (int i = 0; i < d; i++) {
        if (data + i >= end) break;
        values[i] = data[i];
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

    int w, h, d;
    float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
    if (!pixels) {
        fprintf(stderr, "cannot load image '%s'\n", filename.c_str());
        return 0;
    }

    Image* img = new Image(pixels, w, h, (Format) d);
    if (gUseCache) {
        cache[filename] = img;
        img->is_cached = true;
    }
    printf("'%s' loaded\n", filename.c_str());

    watcher_add_file(filename, [](const std::string& filename) {
        if (cache.find(filename) != cache.end()) {
            Image* img = cache[filename];
            for (auto seq : gSequences) {
                seq->forgetImage();
            }
            delete img;
            cache.erase(filename);
        }
        printf("'%s' modified on disk, cache invalidated\n", filename.c_str());
    });

    return img;
}

void Image::flushCache()
{
    for (auto v : cache) {
        delete v.second;
    }
    cache.clear();
    for (auto seq : gSequences) {
        seq->forgetImage();
    }
}

