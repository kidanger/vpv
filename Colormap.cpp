#include <cassert>
#include <string>

#include "imgui.h"

#include "Colormap.hpp"
#include "Shader.hpp"
#include "globals.hpp"

Colormap::Colormap()
{
    static int id = 0;
    id++;
    ID = "Colormap " + std::to_string(id);

    center = .5f,
    radius = .5f;
    shader = gShaders[0];
}

void Colormap::displaySettings()
{
    ImGui::DragFloat("Inverse Contrast", &radius);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the contrast/radius (shift + mouse wheel)");
    ImGui::DragFloat("Inverse Brightness", &center);
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

float Colormap::getScale() const
{
    return 1 / (2.f * radius);
}

float Colormap::getBias() const
{
    float scale = getScale();
    float bias  = (radius - center) * scale;
    return bias;
}

void Colormap::autoCenterAndRadius(float min, float max)
{
    radius = (max - min) / 2.f;
    center = (max + min) / 2.f;
}

void Colormap::getRange(float& min, float& max) const
{
    min = center - radius;
    max = center + radius;
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

