#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "blockingconcurrentqueue.h"

#include "Histogram.hpp"
#include "Image.hpp"

static moodycamel::BlockingConcurrentQueue<std::shared_ptr<Image>> statQueue;

Image::Image(float* pixels, size_t w, size_t h, size_t c)
    : pixels(pixels)
    , w(w)
    , h(h)
    , c(c)
    , lastUsed(0)
    , histogram(std::make_shared<Histogram>())
{
    static int id = 0;
    id++;
    ID = "Image " + std::to_string(id);

    size = ImVec2(w, h);
}

Image::~Image()
{
    free(pixels);
}

void Image::getPixelValueAt(size_t x, size_t y, float* values, size_t d) const
{
    if (x >= w || y >= h)
        return;

    const float* data = (float*)pixels + (w * y + x) * c;
    const float* end = (float*)pixels + (w * h) * c;
    for (size_t i = 0; i < d; i++) {
        if (data + i >= end)
            break;
        values[i] = data[i];
    }
}

std::array<bool, 3> Image::getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const
{
    std::array<bool, 3> valids { false, false, false };
    if (x >= w || y >= h)
        return valids;

    const float* data = (float*)pixels + (w * y + x) * c;
    for (size_t i = 0; i < 3; i++) {
        int b = bands[i];
        if (b >= c)
            continue;
        values[i] = data[b];
        valids[i] = true;
    }
    return valids;
}

void Image::computeStats()
{
    _statsState = STARTED;
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < w * h * c; i++) {
        float v = pixels[i];
        min = std::min(min, v);
        max = std::max(max, v);
    }

    if (!std::isfinite(min) || !std::isfinite(max)) {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < w * h * c; i++) {
            float v = pixels[i];
            if (std::isfinite(v)) {
                min = std::min(min, v);
                max = std::max(max, v);
            }
        }
    }
    _statsState = MINMAX;

    _statsState = DONE;
}

void Image::computeStatsLater()
{
    statQueue.enqueue(shared_from_this());
}

std::shared_ptr<Image> popImageFromStatQueue()
{
    std::shared_ptr<Image> image;
    statQueue.wait_dequeue(image);
    return image;
}
