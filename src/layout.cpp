#include <doctest.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "Image.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "Window.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "layout.hpp"

void nextLayout()
{
    auto& lua = config::get_lua();
    lua["next_layout"]();
    relayout();
}

void previousLayout()
{
    auto& lua = config::get_lua();
    lua["previous_layout"]();
    relayout();
}

void freeLayout()
{
    auto& lua = config::get_lua();
    lua["CURRENT_LAYOUT"] = nullptr;
}

std::string getLayoutName()
{
    auto& lua = config::get_lua();
    if (lua["CURRENT_LAYOUT"].isNilref()) {
        return "free";
    }
    return lua["CURRENT_LAYOUT"];
}

void relayout()
{
    ImVec2 menuPos = ImVec2(0, ImGui::GetFrameHeight() * gShowMenuBar);
    ImVec2 size = ImGui::GetIO().DisplaySize - menuPos;

    auto& lua = config::get_lua();
    ImRect area(menuPos, menuPos + size);
    lua["relayout"](gWindows, area);
}

static std::vector<int> parseCustomLayout(const std::string& str)
{
    std::vector<int> customLayout;
    char* s = const_cast<char*>(str.c_str());
    bool times = false;
    while (*s) {
        if (*s == '!' || *s == '*') {
            s++;
            customLayout.push_back(-1);
        } else {
            char* old = s;
            int n = strtol(s, &s, 10);
            if (times) {
                size_t i = customLayout.size() - 1;
                int v = customLayout[i];
                for (int j = 0; j < n - 1; j++) {
                    customLayout.push_back(v);
                }
                times = false;
            } else {
                customLayout.push_back(n);
            }
            if (s == old)
                break;
            if (*s == 'x') {
                times = true;
            }
        }
        if (*s)
            s++;
    }
    return customLayout;
}

TEST_CASE("layout::parseCustomLayout")
{
    SUBCASE("l:2,8")
    {
        auto l = parseCustomLayout("2,8");
        CHECK(l.size() == 2);
        CHECK(l[0] == 2);
        CHECK(l[1] == 8);
    }
    SUBCASE("l:-1,8")
    {
        auto l = parseCustomLayout("-1,8,-1");
        CHECK(l.size() == 3);
        CHECK(l[0] == -1);
        CHECK(l[1] == 8);
        CHECK(l[2] == -1);
    }
    SUBCASE("l:3x2")
    {
        auto l = parseCustomLayout("3x2");
        CHECK(l.size() == 2);
        CHECK(l[0] == 3);
        CHECK(l[1] == 3);
    }
    SUBCASE("l:5,3x4,8")
    {
        auto l = parseCustomLayout("5,3x4,8");
        CHECK(l.size() == 6);
        CHECK(l[0] == 5);
        CHECK(l[1] == 3);
        CHECK(l[2] == 3);
        CHECK(l[3] == 3);
        CHECK(l[4] == 3);
        CHECK(l[5] == 8);
    }
}

void parseLayout(const std::string& str)
{
    auto& lua = config::get_lua();

    std::string layout;
    if (str == "g" || str == "grid") {
        layout = "grid";
    } else if (str == "f" || str == "fullscreen") {
        layout = "fullscreen";
    } else if (str == "h" || str == "horizontal") {
        layout = "horizontal";
    } else if (str == "v" || str == "vertical") {
        layout = "vertical";
    } else {
        auto customLayout = parseCustomLayout(str);
        if (!customLayout.empty()) {
            layout = "custom";
            lua["CUSTOM_LAYOUT"] = customLayout;
        }
    }

    if (!layout.empty()) {
        lua["CURRENT_LAYOUT"] = layout;
    }
}
