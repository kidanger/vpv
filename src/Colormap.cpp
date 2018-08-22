#include <limits>
#include <cassert>
#include <string>

#include "imgui.h"

#include "Colormap.hpp"
#include "Shader.hpp"
#include "shaders.hpp"
#include "globals.hpp"

Colormap::Colormap()
{
    static int id = 0;
    id++;
    ID = "Colormap " + std::to_string(id);

    for (int i = 0; i < 3; i++)
        center[i] = .5f,
    radius = .5f;
    shader = nullptr;
    initialized = false;
}

void Colormap::displaySettings()
{
    ImGui::DragFloat("Inverse Contrast", &radius);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the contrast/radius (shift + mouse wheel)");
    ImGui::DragFloat3("Inverse Brightness", &center[0]);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the brightness/center (mouse wheel)");

    const char* items[gShaders.size()];
    for (int i = 0; i < gShaders.size(); i++)
        items[i] = gShaders[i]->name.c_str();
    int index = 0;
    while (shader != gShaders[index]) index++;
    ImGui::Combo("Tonemap", &index, items, gShaders.size());
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the shader (s / shift+s)");
    shader = gShaders[index];
}

std::array<float, 3> Colormap::getScale() const
{
    std::array<float,3> scale;
    for (int i = 0; i < 3; i++)
        scale[i] = 1 / (2.f * radius);
    return scale;
}

std::array<float, 3> Colormap::getBias() const
{
    std::array<float,3> bias;
    std::array<float,3> scale = getScale();
    for (int i = 0; i < 3; i++)
        bias[i] = (radius - center[i]) * scale[i];
    return bias;
}

void Colormap::autoCenterAndRadius(float min, float max)
{
    radius = (max - min) / 2.f;
    for (int i = 0; i < 3; i++)
        center[i] = (max + min) / 2.f;
}

void Colormap::getRange(float& min, float& max, int n) const
{
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::min();
    for (int i = 0; i < n; i++) {
        min = std::min(min, center[i] - radius);
        max = std::max(max, center[i] + radius);
    }
}

void Colormap::nextShader()
{
    for (int i = 0; i < gShaders.size() - 1; i++) {
        if (gShaders[i] == shader) {
            shader = gShaders[i + 1];
            return;
        }
    }
    shader = gShaders[0];
}

void Colormap::previousShader()
{
    for (int i = 1; i < gShaders.size(); i++) {
        if (gShaders[i] == shader) {
            shader = gShaders[i - 1];
            return;
        }
    }
    shader = gShaders[gShaders.size() - 1];
}

std::string Colormap::getShaderName() const
{
    return shader->name;
}

