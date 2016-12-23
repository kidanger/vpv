#include <cassert>
#include <string>

#include "imgui.h"

#include "Colormap.hpp"
#include "shaders.hpp"

Colormap::Colormap()
{
    static int id = 0;
    id++;
    ID = "Colormap " + std::to_string(id);

    scale = 1.f,
    bias = 0.f;
    tonemap = RGB;
}

void Colormap::displaySettings()
{
    ImGui::DragFloat("Contrast", &scale);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the contrast/scale (shift + mouse wheel)");
    ImGui::DragFloat("Brightness", &bias);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the brightness/bias (mouse wheel)");

    const char* items[NUM_TONEMAPS] = {"Gray", "RGB", "Optical flow", "Jet"};
    ImGui::Combo("Tonemap", (int*) &tonemap, items, gShaders.size());
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the shader (s / shift+s)");
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

