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

std::unordered_map<std::string, Image*> Image::cache;
static std::mutex lock;

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
    size = ImVec2(w, h);
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
    lock.lock();
    auto i = cache.find(filename);
    if (i != cache.end()) {
        Image* img = i->second;
        lock.unlock();
        return img;
    }
    if (!force_load) {
        lock.unlock();
        return 0;
    }
    lock.unlock();

    int w, h, d;
    float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
    if (!pixels) {
        fprintf(stderr, "cannot load image '%s'\n", filename.c_str());
        return 0;
    }

    if (d > 4) {
        printf("warning: '%s' has %d channels, extracting the first four\n", filename.c_str(), d);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                for (int l = 0; l < 4; l++) {
                    pixels[(y*h+x)*4+l] = pixels[(y*h+x)*d+l];
                }
            }
        }
        d = 4;
    }

    Image* img = new Image(pixels, w, h, (Format) d);
    if (gUseCache) {
        lock.lock();
        auto i = cache.find(filename);
        if (i != cache.end()) {
            // time flies when you're having fun
            // looks like someone else loaded it already
            delete img;
            img = i->second;
            lock.unlock();
            return img;
        }
        cache[filename] = img;
        lock.unlock();
        img->is_cached = true;
    }
    printf("'%s' loaded\n", filename.c_str());

    watcher_add_file(filename, [](const std::string& filename) {
        lock.lock();
        if (cache.find(filename) != cache.end()) {
            Image* img = cache[filename];
            for (auto seq : gSequences) {
                seq->forgetImage();
            }
            delete img;
            cache.erase(filename);
        }
        lock.unlock();
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

