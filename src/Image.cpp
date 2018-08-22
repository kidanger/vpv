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

std::unordered_map<std::string, Image*> Image::cache;
static std::mutex lock;
static size_t cacheSize = 0;

Image::Image(float* pixels, int w, int h, Format format)
    : pixels(pixels), w(w), h(h), format(format), is_cached(false), lastUsed(0)
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

void Image::computeHistogram(float min, float max)
{
    if (!histograms.empty() && histmin == min && histmax == max)
        return;

    histograms.clear();
    histograms.resize(format);

    // TODO: oversample the histogram and resample on the fly

    const int nbins = 256;
    for (int d = 0; d < format; d++) {
        auto& histogram = histograms[d];
        histogram.clear();
        histogram.resize(nbins);

        float f = nbins / (max - min);
        for (int i = 0; i < w*h; i++) {
            int bin = (pixels[i*format+d] - min) * f;
            if (bin >= 0 && bin < nbins) {
                histogram[bin]++;
            }
        }
    }
    histmin = min;
    histmax = max;
}

/*
Image* Image::load(const std::string& filename, bool force_load)
{
    lock.lock();
    auto i = cache.find(filename);
    if (i != cache.end()) {
        Image* img = i->second;
        lock.unlock();
        if (force_load) {
            letTimeFlow(&img->lastUsed);
        }
        return img;
    }
    if (!force_load) {
        lock.unlock();
        return 0;
    }
    lock.unlock();

    while (cacheSize / 1000000 > gCacheLimitMB) {
        lock.lock();

        std::string worst;
        const Image* worstImg;
        double last = 0;
        for (auto it = cache.begin(); it != cache.end(); it++) {
            Image* img = it->second;
            uint64_t t = img->lastUsed;
            double imgtime = letTimeFlow(&t);
            bool used = false;
            for (auto seq : gSequences) {
                if (seq->image == img) {
                    used = true;
                    break;
                }
            }
            if (imgtime > last && !used) {
                worst = it->first;
                worstImg = img;
                last = imgtime;
            }
        }

        if (!worst.empty()) {
            cache.erase(worst);
            cacheSize -= worstImg->w * worstImg->h * worstImg->format * sizeof(float);
            gPreload = false;
        } else {
            printf("Sorry I wasn't able to free enough memory to load the image '%s'\n", filename.c_str());
            printf("I need to exit to avoid saturating your RAM.\n");
            printf("Increase the setting 'CACHE_LIMIT' and see if the problem persists.\n");
            exit(1);
        }

        lock.unlock();
    }

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
        cacheSize += img->w * img->h * img->format * sizeof(float);
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
            cacheSize -= img->w * img->h * img->format * sizeof(float);
        }
        lock.unlock();
        printf("'%s' modified on disk, cache invalidated\n", filename.c_str());
    });

    letTimeFlow(&img->lastUsed);
    return img;
}
*/

void Image::flushCache()
{
    lock.lock();
    for (auto v : cache) {
        delete v.second;
    }
    cache.clear();
    cacheSize = 0;
    lock.unlock();
    for (auto seq : gSequences) {
        seq->forgetImage();
    }
}

#include "ImageProvider.hpp"

SingleImageImageCollection::SingleImageImageCollection(const std::string& filename) : filename(filename) {
    ImageProvider* provider = new IIOFileImageProvider(filename);
    this->provider = new CacheImageProvider(provider, filename);
}
