#include <limits>
#include <cassert>
#include <string>

#include "imgui.h"

#include "Colormap.hpp"
#include "Shader.hpp"
#include "Sequence.hpp"
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
    currentSat = 0;
    bands[0] = 0;
    bands[1] = 1;
    bands[2] = 2;
}

void Colormap::displaySettings()
{
    ImGui::DragFloat("Inverse Contrast", &radius);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the contrast/radius (shift + mouse wheel)");
    ImGui::DragFloat3("Inverse Brightness", &center[0]);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the brightness/center (mouse wheel)");

    std::vector<const char*> items(gShaders.size());
    for (int i = 0; i < gShaders.size(); i++)
        items[i] = gShaders[i]->name.c_str();
    int index = 0;
    while (shader != gShaders[index]) index++;
    ImGui::Combo("Tonemap", &index, &items[0], gShaders.size());
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the shader (s / shift+s)");
    shader = gShaders[index];

    size_t vmax = 0;
    for (auto seq : gSequences) {
        if (seq->image)
            vmax = std::max(vmax, seq->image->c-1);
    }
    int vals[3] = {(int)bands[0], (int)bands[1], (int)bands[2]};
    ImGui::SliderInt3("Bands", vals, -1, vmax);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Select the bands for the 'red', 'blue' and 'green' channels. -1 to disable a channel.");
    for (int i = 0; i < 3; i++) {
        bands[i] = vals[i];
    }
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
    if (min >= max) {
        radius = 0.5f;
    } else {
        radius = (max - min) / 2.f;
    }
    for (int i = 0; i < 3; i++)
        center[i] = (max + min) / 2.f;
}

void Colormap::getRange(float& min, float& max, int n) const
{
    min = std::numeric_limits<float>::max();
    max = std::numeric_limits<float>::lowest();
    for (int i = 0; i < std::min<size_t>(n, center.size()); i++) {
        min = std::min(min, center[i] - radius);
        max = std::max(max, center[i] + radius);
    }
}

void Colormap::getRange(std::array<float,3>& min, std::array<float,3>& max) const
{
    for (int i = 0; i < 3; i++) {
        min[i] = center[i] - radius;
        max[i] = center[i] + radius;
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

const std::string& Colormap::getShaderName() const
{
    return shader->name;
}

bool Colormap::setShader(const std::string& name)
{
    Shader* s = getShader(name);
    if (s) {
        shader = s;
    }
    return s;
}

