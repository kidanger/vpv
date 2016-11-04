#pragma once

#include <string>

#include "imgui.h"

namespace sf {
  class Texture;
}

struct View {
    std::string ID;

    float zoom;
    float smallzoomfactor;
    ImVec2 center;

    View();

    void compute(const ImVec2& texSize, ImVec2& u, ImVec2& v) const;

    void displaySettings();

};

