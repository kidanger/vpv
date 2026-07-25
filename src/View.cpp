#include <cmath>
#include <doctest.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "View.hpp"
#include "globals.hpp"
#include "strutils.hpp"

View::View()
{
    static int id = 0;
    id++;
    ID = "View " + std::to_string(id);

    zoom = 1.f;
    center = ImVec2(0.5f, 0.5f);
    rotation = 0.f;
    shouldRescale = false;
    svgOffset = ImVec2(0.f, 0.f);
}

bool View::operator==(const View& other)
{
    return other.ID == ID;
}

void View::resetZoom()
{
    changeZoom(1.f);
    rotation = 0.f;
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
    rotation = 0.f;
}

ImVec2 View::image2window(const ImVec2& im, const ImVec2& imSize, const ImVec2& winSize, float zoomfactor) const
{
    ImVec2 p = (im - center * imSize) * zoom * zoomfactor;
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    return ImVec2(c * p.x - s * p.y, s * p.x + c * p.y) + winSize / 2.f;
}

ImVec2 View::window2image(const ImVec2& win, const ImVec2& imSize, const ImVec2& winSize, float zoomfactor) const
{
    ImVec2 p = win - winSize / 2.f;
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    ImVec2 unrotated(c * p.x + s * p.y, -s * p.x + c * p.y);
    return center * imSize + unrotated / (zoom * zoomfactor);
}

void View::displaySettings()
{
    ImGui::DragFloat("Zoom", &zoom, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::SameLine();
    ImGui::ShowHelpMarker("Change the zoom (z+mouse wheel, i or o)");
    ImGui::DragFloat2("Center", &center.x, 0.f, 1.f);
    ImGui::SameLine();
    ImGui::ShowHelpMarker("Scroll the image (left click + drag)");
    float rotDeg = rotation * (180.f / M_PI);
    if (ImGui::DragFloat("Rotation (deg)", &rotDeg, 0.5f, -180.f, 180.f, "%.1f")) {
        rotation = rotDeg * (M_PI / 180.f);
    }
    ImGui::SameLine();
    ImGui::ShowHelpMarker("Rotate the image (ctrl+alt+mouse wheel, or middle click + vertical drag)");
    ImGui::Checkbox("Scale for each sequence", &shouldRescale);
    ImGui::DragFloat2("SVG offset", &svgOffset.x, 0.f, 1.f);
}

bool View::parseArg(const std::string& arg)
{
    if (startswith(arg, "v:zoom:")) {
        float z = 1.f;
        if (sscanf(arg.c_str(), "v:zoom:%f", &z) == 1) {
            zoom = z;
            return true;
        }
    } else if (startswith(arg, "v:center:")) {
        float x = 0.5f;
        float y = 0.5f;
        if (sscanf(arg.c_str(), "v:center:%f,%f", &x, &y) == 2) {
            center = ImVec2(x, y);
            return true;
        }
    } else if (startswith(arg, "v:rotation:")) {
        float r = 0.f;
        if (sscanf(arg.c_str(), "v:rotation:%f", &r) == 1) {
            rotation = r * (M_PI / 180.f);
            return true;
        }
    } else if (startswith(arg, "v:svgoffset:")) {
        float x = 0.f;
        float y = 0.f;
        if (sscanf(arg.c_str(), "v:svgoffset:%f,%f", &x, &y) == 2) {
            svgOffset = ImVec2(x, y);
            return true;
        }
    } else if (arg[2] == 's') {
        shouldRescale = true;
        return true;
    }
    return false;
}

TEST_CASE("View::parseArg")
{
    View v;

    SUBCASE("v:s")
    {
        CHECK(!v.shouldRescale);
        CHECK(v.parseArg("v:s"));
        CHECK(v.shouldRescale);
        CHECK(v.parseArg("v:s"));
        CHECK(v.shouldRescale);
    }

    SUBCASE("v:zoom")
    {
        CHECK(v.zoom == doctest::Approx(1.f));
        CHECK(v.parseArg("v:zoom:20"));
        CHECK(v.zoom == doctest::Approx(20.f));
        SUBCASE("v:zoom invalid")
        {
            CHECK(!v.parseArg("v:zoom:aa"));
            CHECK(v.zoom == doctest::Approx(20.f));
        }
    }

    SUBCASE("v:center")
    {
        CHECK(v.center[0] == doctest::Approx(0.5f));
        CHECK(v.center[1] == doctest::Approx(0.5f));
        CHECK(v.parseArg("v:center:2,-3.5"));
        CHECK(v.center[0] == doctest::Approx(2.f));
        CHECK(v.center[1] == doctest::Approx(-3.5f));
        SUBCASE("v:center invalid")
        {
            CHECK(!v.parseArg("v:center:1"));
            CHECK(v.center[0] == doctest::Approx(2.f));
            CHECK(v.center[1] == doctest::Approx(-3.5f));
        }
    }

    SUBCASE("v:svgoffset")
    {
        CHECK(v.svgOffset[0] == doctest::Approx(0.f));
        CHECK(v.svgOffset[1] == doctest::Approx(0.f));
        CHECK(v.parseArg("v:svgoffset:2,-3.5"));
        CHECK(v.svgOffset[0] == doctest::Approx(2.f));
        CHECK(v.svgOffset[1] == doctest::Approx(-3.5f));
        SUBCASE("v:svgoffset invalid")
        {
            CHECK(!v.parseArg("v:svgoffset:1"));
            CHECK(v.svgOffset[0] == doctest::Approx(2.f));
            CHECK(v.svgOffset[1] == doctest::Approx(-3.5f));
        }
    }

    SUBCASE("v:rotation")
    {
        CHECK(v.rotation == doctest::Approx(0.f));
        CHECK(v.parseArg("v:rotation:90"));
        CHECK(v.rotation == doctest::Approx(M_PI / 2.f));
        CHECK(v.parseArg("v:rotation:-180"));
        CHECK(v.rotation == doctest::Approx(-M_PI));
        SUBCASE("v:rotation invalid")
        {
            CHECK(!v.parseArg("v:rotation:aa"));
            CHECK(v.rotation == doctest::Approx(-M_PI));
        }
    }
}
