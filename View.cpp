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
}

void View::resetZoom()
{
    changeZoom(1.f);
}

void View::changeZoom(float zoom)
{
    this->zoom = zoom;
}

void View::setOptimalZoom(ImVec2 winSize, ImVec2 texSize)
{
    float w = winSize.x;
    float h = winSize.y;
    float sw = texSize.x;
    float sh = texSize.y;
    changeZoom(std::min(w / sw, h / sh));
}

void View::compute(const ImVec2& texSize, ImVec2& u, ImVec2& v) const
{
    u = center / texSize - 1 / (2 * zoom);
    v = center / texSize + 1 / (2 * zoom);
}

ImVec2 View::image2window(const ImVec2& im, const ImVec2& imSize, const ImVec2& winSize) const
{
    return (im - center * imSize) * zoom + winSize / 2.f;
}

ImVec2 View::window2image(const ImVec2& win, const ImVec2& imSize, const ImVec2& winSize) const
{
    return center * imSize + win / zoom - winSize / (2.f * zoom);
}

void View::displaySettings() {
    ImGui::DragFloat("Zoom", &zoom, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the zoom (z+mouse wheel, i or o)");
    ImGui::DragFloat2("Center", &center.x, 0.f, 1.f);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Scroll the image (left click + drag)");
}

