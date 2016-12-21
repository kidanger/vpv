#include <SFML/Graphics/Texture.hpp>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "View.hpp"

View::View()
{
    static int id = 0;
    id++;
    ID = "View " + std::to_string(id);

    zoom = 1.f;
    smallzoomfactor = 30.f;
}

void View::resetZoom()
{
    changeZoom(1.f);
}

void View::changeZoom(float zoom)
{
    this->zoom = zoom;
    printf("Zoom: %g\n", zoom);
}

void View::setOptimalZoom(ImVec2 winSize, ImVec2 texSize)
{
    float w = winSize.x;
    float h = winSize.y;
    float sw = texSize.x;
    float sh = texSize.y;
    changeZoom(std::min(h/w*sw/sh, w/h*sh/sw));
}

void View::compute(const ImVec2& texSize, ImVec2& u, ImVec2& v) const
{
    u = center / texSize - 1 / (2 * zoom);
    v = center / texSize + 1 / (2 * zoom);
}

void View::displaySettings() {
    ImGui::DragFloat("Zoom", &zoom, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::DragFloat("Tooltip zoom factor", &smallzoomfactor, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::DragFloat2("Center", &center.x, 0.f, 100.f);
}

