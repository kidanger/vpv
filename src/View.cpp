#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "View.hpp"
#include "globals.hpp"

View::View()
{
    static int id = 0;
    id++;
    ID = "View " + std::to_string(id);

    zoom = 1.f;
    center = ImVec2(0.5, 0.5);
    shouldRescale = false;
    svgOffset = gDefaultSvgOffset;
}

void View::resetZoom()
{
    changeZoom(1.f);
}

void View::changeZoom(float zoom)
{
    this->zoom = zoom;
}

void View::setOptimalZoom(ImVec2 winSize, ImVec2 imSize, float zoomfactor)
{
    float w = winSize.x;
    float h = winSize.y;
    float sw = imSize.x * zoomfactor;
    float sh = imSize.y * zoomfactor;
    changeZoom(std::min(w / sw, h / sh));
}

ImVec2 View::image2window(const ImVec2& im, const ImVec2& imSize, const ImVec2& winSize, float zoomfactor) const
{
    return (im - center * imSize) * zoom * zoomfactor + winSize / 2.f;
}

ImVec2 View::window2image(const ImVec2& win, const ImVec2& imSize, const ImVec2& winSize, float zoomfactor) const
{
    return center * imSize + win / (zoom*zoomfactor) - winSize / (2.f * zoom * zoomfactor);
}

void View::displaySettings()
{
    ImGui::DragFloat("Zoom", &zoom, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the zoom (z+mouse wheel, i or o)");
    ImGui::DragFloat2("Center", &center.x, 0.f, 1.f);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Scroll the image (left click + drag)");
    ImGui::Checkbox("Scale for each sequence", &shouldRescale);
    ImGui::DragFloat2("SVG offset", &svgOffset.x, 0.f, 1.f);
}

