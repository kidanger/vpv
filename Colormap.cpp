#include <cassert>
#include <string>

#include "Colormap.hpp"

Colormap::Colormap()
{
    scale = 1.f,
    bias = 0.f;
    tonemap = RGB;
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
        default:
            assert(false);
    }
}

