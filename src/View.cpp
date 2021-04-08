#include <doctest.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "strutils.hpp"
#include "View.hpp"
#include "globals.hpp"

View::View()
{
    static int id = 0;
    id++;
    ID = "View " + std::to_string(id);

    zoom = 1.f;
    center = ImVec2(0.5f, 0.5f);
    shouldRescale = false;
    svgOffset = ImVec2(0.f, 0.f);
}

bool View::operator==(const View& other) {
    return other.ID == ID;
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

TEST_CASE("View::parseArg") {
    View v;

    SUBCASE("v:s") {
        CHECK(!v.shouldRescale);
        CHECK(v.parseArg("v:s"));
        CHECK(v.shouldRescale);
        CHECK(v.parseArg("v:s"));
        CHECK(v.shouldRescale);
    }

    SUBCASE("v:zoom") {
        CHECK(v.zoom == doctest::Approx(1.f));
        CHECK(v.parseArg("v:zoom:20"));
        CHECK(v.zoom == doctest::Approx(20.f));
        SUBCASE("v:zoom invalid") {
            CHECK(!v.parseArg("v:zoom:aa"));
            CHECK(v.zoom == doctest::Approx(20.f));
        }
    }

    SUBCASE("v:center") {
        CHECK(v.center[0] == doctest::Approx(0.5f));
        CHECK(v.center[1] == doctest::Approx(0.5f));
        CHECK(v.parseArg("v:center:2,-3.5"));
        CHECK(v.center[0] == doctest::Approx(2.f));
        CHECK(v.center[1] == doctest::Approx(-3.5f));
        SUBCASE("v:center invalid") {
            CHECK(!v.parseArg("v:center:1"));
            CHECK(v.center[0] == doctest::Approx(2.f));
            CHECK(v.center[1] == doctest::Approx(-3.5f));
        }
    }

    SUBCASE("v:svgoffset") {
        CHECK(v.svgOffset[0] == doctest::Approx(0.f));
        CHECK(v.svgOffset[1] == doctest::Approx(0.f));
        CHECK(v.parseArg("v:svgoffset:2,-3.5"));
        CHECK(v.svgOffset[0] == doctest::Approx(2.f));
        CHECK(v.svgOffset[1] == doctest::Approx(-3.5f));
        SUBCASE("v:svgoffset invalid") {
            CHECK(!v.parseArg("v:svgoffset:1"));
            CHECK(v.svgOffset[0] == doctest::Approx(2.f));
            CHECK(v.svgOffset[1] == doctest::Approx(-3.5f));
        }
    }
}

