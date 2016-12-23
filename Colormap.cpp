#include <cassert>
#include <string>

#include "Colormap.hpp"

Colormap::Colormap()
{
    scale = 1.f,
    bias = 0.f;
    tonemap = RGB;
}

void Colormap::getRange(float& min, float& max) const
{
    min = 255*bias;
    max = 255*(bias + 255*scale);
}

void Colormap::print() const
{
    float min, max;
    getRange(min, max);
    printf("Colormap: map to [%.5f..%.5f], shader: %s\n", min, max, getShaderName().c_str());
}

std::string Colormap::getShaderName() const
{
    switch (tonemap) {
        case GRAY:
            return "gray";
        case RGB:
            return "default";
        case OPTICAL_FLOW:
            return "opticalFlow";
        case JET:
            return "jet";
        default:
            assert(false);
    }
}

