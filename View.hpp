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

    void resetZoom();
    void changeZoom(float zoom);
    void setOptimalZoom(ImVec2 winSize, ImVec2 texSize);

    void compute(const ImVec2& texSize, ImVec2& u, ImVec2& v) const;

    void displaySettings();

};

