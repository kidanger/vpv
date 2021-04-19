#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <doctest.h>
#include <imgui.h>

#include "strutils.hpp"
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

    for (int i = 0; i < 3; i++) {
        center[i] = .5f;
    }
    radius = .5f;
    shader = nullptr;
    initialized = false;
    currentSat = 0;
    bands[0] = 0;
    bands[1] = 1;
    bands[2] = 2;
}

bool Colormap::operator==(const Colormap& other) {
    return other.ID == ID;
}

void Colormap::displaySettings()
{
    ImGui::DragFloat("Inverse Contrast", &radius);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the contrast/radius (shift + mouse wheel)");
    ImGui::DragFloat3("Inverse Brightness", center.data());
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the brightness/center (mouse wheel)");

    std::vector<const char*> items(gShaders.size());
    for (int i = 0; i < gShaders.size(); i++)
        items[i] = gShaders[i]->getName().c_str();
    int index = 0;
    while (shader && shader != gShaders[index]) index++;
    ImGui::Combo("Tonemap", &index, items.data(), gShaders.size());
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the shader (s / shift+s)");
    shader = gShaders[index];

    std::vector<std::weak_ptr<Sequence>> to_remove;
    size_t vmax = 0;
    for (const auto& ptr : sequences) {
        if (const auto& seq = ptr.lock()) {
            if (seq->image)
                vmax = std::max(vmax, seq->image->c - 1);
        } else {
            to_remove.push_back(ptr);
        }
    }
    for (const auto& ptr : to_remove) {
        sequences.erase(ptr);
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
    if (shader) {
        return shader->getName();
    }
    static std::string null;
    return null;
}

bool Colormap::setShader(const std::string& name)
{
    const auto& s = getShader(name);
    if (s) {
        shader = s;
        return true;
    }

    return false;
}

void Colormap::onSequenceAttach(std::weak_ptr<Sequence> s)
{
    sequences.insert(std::move(s));
}

void Colormap::onSequenceDetach(std::weak_ptr<Sequence> s)
{
    sequences.erase(std::move(s));
}

bool Colormap::parseArg(const std::string& arg)
{
    if (startswith(arg, "c:bands:")) {
        int b0 = -1, b1 = -1, b2 = -1;
        sscanf(arg.c_str(), "c:bands:%d,%d,%d", &b0, &b1, &b2);
        bands[0] = b0;
        bands[1] = b1;
        bands[2] = b2;
        return true;
    }
    return false;
}

TEST_CASE("Colormap::parseArg") {
    Colormap c;

    SUBCASE("c:bands") {
        CHECK(c.bands[0] == 0);
        CHECK(c.bands[1] == 1);
        CHECK(c.bands[2] == 2);
        CHECK(c.parseArg("c:bands:8,-1,2"));
        CHECK(c.bands[0] == 8);
        CHECK(c.bands[1] == -1);
        CHECK(c.bands[2] == 2);
        CHECK(c.parseArg("c:bands:1,2,3"));
        CHECK(c.bands[0] == 1);
        CHECK(c.bands[1] == 2);
        CHECK(c.bands[2] == 3);
    }

    SUBCASE("c:bands partial") {
        CHECK(c.parseArg("c:bands:1,3"));
        CHECK(c.bands[0] == 1);
        CHECK(c.bands[1] == 3);
        CHECK(c.bands[2] == -1);
        CHECK(c.parseArg("c:bands:9"));
        CHECK(c.bands[0] == 9);
        CHECK(c.bands[1] == -1);
        CHECK(c.bands[2] == -1);
    }
}

