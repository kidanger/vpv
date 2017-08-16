#pragma once

#include <string>

#include "imgui.h"

namespace sf {
  class Texture;
}

struct View {
    std::string ID;

    float zoom;
    ImVec2 center;

    View();

    void resetZoom();
    void changeZoom(float zoom);
    void setOptimalZoom(ImVec2 winSize, ImVec2 texSize);

    void compute(const ImVec2& texSize, ImVec2& u, ImVec2& v) const;

    ImVec2 image2window(const ImVec2& im, const ImVec2& imSize, const ImVec2& winSize) const;
    ImVec2 window2image(const ImVec2& win, const ImVec2& imSize, const ImVec2& winSize) const;

    void displaySettings();
};

