#pragma once

#include <string>

struct Colormap
{
    float scale;
    float bias;

    enum Tonemap {
        GRAY, RGB, OPTICAL_FLOW, JET,
        NUM_TONEMAPS
    } tonemap;

    Colormap();

    std::string getShaderName() const;
};

