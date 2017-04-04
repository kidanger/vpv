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

    scale = 1.f,
    bias = 0.f;
    shader = gShaders[0];
}

void Colormap::displaySettings()
{
    ImGui::DragFloat("Contrast", &scale);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the contrast/scale (shift + mouse wheel)");
    ImGui::DragFloat("Brightness", &bias);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the brightness/bias (mouse wheel)");

    const char* items[gShaders.size()];
    for (int i = 0; i < gShaders.size(); i++)
        items[i] = gShaders[i]->name.c_str();
    int index = 0;
    while (shader != gShaders[index]) index++;
    ImGui::Combo("Tonemap", &index, items, gShaders.size());
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the shader (s / shift+s)");
    shader = gShaders[index];
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

