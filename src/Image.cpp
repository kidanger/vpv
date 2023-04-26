#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "Histogram.hpp"
#include "Image.hpp"

Image::Image(float* pixels, size_t w, size_t h, size_t c)
    : Image(pixels, w, h, c, F32)
{
}

Image::Image(void* pixels, size_t w, size_t h, size_t c, Format format)
    : w(w)
    , h(h)
    , c(c)
    , format(format)
    , lastUsed(0)
    , histogram(std::make_shared<Histogram>())
    , pixels(pixels)
{
    static int id = 0;
    id++;
    ID = "Image " + std::to_string(id);

    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < w * h * c; i++) {
        float v = at(i);
        min = std::min(min, v);
        max = std::max(max, v);
    }
    if (!std::isfinite(min) || !std::isfinite(max)) {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < w * h * c; i++) {
            float v = at(i);
            if (std::isfinite(v)) {
                min = std::min(min, v);
                max = std::max(max, v);
            }
        }
    }
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

    for (size_t i = 0; i < 3; i++) {
        int b = bands[i];
        if (b >= c)
            continue;
        values[i] = at(x, y, b);
        valids[i] = true;
    }
    return valids;
}

std::pair<float, float> Image::minmax_in_rect(BandIndices bands, ImVec2 p1, ImVec2 p2) const
{
    float low = std::numeric_limits<float>::max();
    float high = std::numeric_limits<float>::lowest();
    for (int d = 0; d < 3; d++) {
        int b = bands[d];
        if (b >= c)
            continue;
        for (int y = p1.y; y < p2.y; y++) {
            for (int x = p1.x; x < p2.x; x++) {
                float v = at(x, y, b);
                if (std::isfinite(v)) {
                    low = std::min(low, v);
                    high = std::max(high, v);
                }
            }
        }
    }
    return std::make_pair(low, high);
}

std::pair<float, float> Image::quantiles(BandIndices bands, float quantile) const
{
    std::vector<float> all;
    if (c <= 3 && bands == BANDS_DEFAULT && format == F32) {
        // fast path
        auto fpixels = reinterpret_cast<float*>(pixels);
        all = std::vector<float>(fpixels, fpixels + w * h * c);
    } else {
        for (int d = 0; d < 3; d++) {
            int b = bands[d];
            if (b >= c)
                continue;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    float v = at(x, y, b);
                    all.push_back(v);
                }
            }
        }
    }

    all.erase(std::remove_if(all.begin(), all.end(),
                  [](float x) { return !std::isfinite(x); }),
        all.end());
    std::sort(all.begin(), all.end());
    float q0 = all[quantile * all.size()];
    float q1 = all[(1 - quantile) * all.size()];

    return std::make_pair(q0, q1);
}

std::pair<float, float> Image::quantiles_in_rect(BandIndices bands, float quantile, ImVec2 p1, ImVec2 p2) const
{
    std::vector<float> all;

    if (c <= 3 && bands == BANDS_DEFAULT && format == F32) {
        // fast path
        auto fpixels = reinterpret_cast<float*>(pixels);
        for (int y = p1.y; y < p2.y; y++) {
            const float* start = &fpixels[0 + c * ((int)p1.x + y * w)];
            const float* end = &fpixels[0 + c * ((int)p2.x + y * w)];
            all.insert(all.end(), start, end);
        }
    } else {
        for (int d = 0; d < 3; d++) {
            int b = bands[d];
            if (b >= c)
                continue;
            for (int y = p1.y; y < p2.y; y++) {
                for (int x = p1.x; x < p2.x; x++) {
                    float v = at(x, y, b);
                    all.push_back(v);
                }
            }
        }
    }

    all.erase(std::remove_if(all.begin(), all.end(),
                  [](float x) { return !std::isfinite(x); }),
        all.end());
    std::sort(all.begin(), all.end());
    float q0 = all[quantile * all.size()];
    float q1 = all[(1 - quantile) * all.size()];

    return std::make_pair(q0, q1);
}

const float* Image::extract_into_glbuffer(BandIndices bands, ImRect intersect, float* reshapebuffer, size_t tw, size_t th, size_t max_size, bool needsreshape) const
{
    if (!needsreshape) {
        assert(c <= 3);
        assert(format == F32);
        size_t offset = index(intersect.Min.x, intersect.Min.y, 0);
        return reinterpret_cast<float*>(pixels) + offset;
    } else {
        // NOTE: all this copy and upload is slow
        // 1) use opengl buffer to avoid pausing at each tile's upload
        // 2) prepare the reshapebuffers in a thread
        // storing these images as planar would help with cache
        switch (format) {
        case U8: {
            uint8_t* pxls = reinterpret_cast<uint8_t*>(pixels);
            for (int cc = 0; cc < 3; cc++) {
                size_t b = bands[cc];
                if (b >= c) {
                    for (int y = 0; y < th; y++) {
                        for (int x = 0; x < tw; x++) {
                            reshapebuffer[(y * max_size + x) * 3 + cc] = 0;
                        }
                    }
                    continue;
                }
                int sx = intersect.Min.x;
                int sy = intersect.Min.y;
                for (int y = 0; y < intersect.GetHeight(); y++) {
                    for (int x = 0; x < intersect.GetWidth(); x++) {
                        reshapebuffer[(y * max_size + x) * 3 + cc] = pxls[index(sx + x, sy + y, b)];
                    }
                }
            }
        } break;
        case I8: {
            int8_t* pxls = reinterpret_cast<int8_t*>(pixels);
            for (int cc = 0; cc < 3; cc++) {
                size_t b = bands[cc];
                if (b >= c) {
                    for (int y = 0; y < th; y++) {
                        for (int x = 0; x < tw; x++) {
                            reshapebuffer[(y * max_size + x) * 3 + cc] = 0;
                        }
                    }
                    continue;
                }
                int sx = intersect.Min.x;
                int sy = intersect.Min.y;
                for (int y = 0; y < intersect.GetHeight(); y++) {
                    for (int x = 0; x < intersect.GetWidth(); x++) {
                        reshapebuffer[(y * max_size + x) * 3 + cc] = pxls[index(sx + x, sy + y, b)];
                    }
                }
            }
        } break;
        case U16: {
            uint16_t* pxls = reinterpret_cast<uint16_t*>(pixels);
            for (int cc = 0; cc < 3; cc++) {
                size_t b = bands[cc];
                if (b >= c) {
                    for (int y = 0; y < th; y++) {
                        for (int x = 0; x < tw; x++) {
                            reshapebuffer[(y * max_size + x) * 3 + cc] = 0;
                        }
                    }
                    continue;
                }
                int sx = intersect.Min.x;
                int sy = intersect.Min.y;
                for (int y = 0; y < intersect.GetHeight(); y++) {
                    for (int x = 0; x < intersect.GetWidth(); x++) {
                        reshapebuffer[(y * max_size + x) * 3 + cc] = pxls[index(sx + x, sy + y, b)];
                    }
                }
            }
        } break;
        case I16: {
            int16_t* pxls = reinterpret_cast<int16_t*>(pixels);
            for (int cc = 0; cc < 3; cc++) {
                size_t b = bands[cc];
                if (b >= c) {
                    for (int y = 0; y < th; y++) {
                        for (int x = 0; x < tw; x++) {
                            reshapebuffer[(y * max_size + x) * 3 + cc] = 0;
                        }
                    }
                    continue;
                }
                int sx = intersect.Min.x;
                int sy = intersect.Min.y;
                for (int y = 0; y < intersect.GetHeight(); y++) {
                    for (int x = 0; x < intersect.GetWidth(); x++) {
                        reshapebuffer[(y * max_size + x) * 3 + cc] = pxls[index(sx + x, sy + y, b)];
                    }
                }
            }
        } break;
        case F32: {
            float* pxls = reinterpret_cast<float*>(pixels);
            for (int cc = 0; cc < 3; cc++) {
                size_t b = bands[cc];
                if (b >= c) {
                    for (int y = 0; y < th; y++) {
                        for (int x = 0; x < tw; x++) {
                            reshapebuffer[(y * max_size + x) * 3 + cc] = 0;
                        }
                    }
                    continue;
                }
                int sx = intersect.Min.x;
                int sy = intersect.Min.y;
                for (int y = 0; y < intersect.GetHeight(); y++) {
                    for (int x = 0; x < intersect.GetWidth(); x++) {
                        reshapebuffer[(y * max_size + x) * 3 + cc] = pxls[index(sx + x, sy + y, b)];
                    }
                }
            }
        } break;
        }
    }
    return reshapebuffer;
}
