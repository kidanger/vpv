#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "layout.hpp"
#include "Sequence.hpp"
#include "Window.hpp"
#include "View.hpp"
#include "Image.hpp"
#include "globals.hpp"
#include "config.hpp"

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
    ImVec2 menuPos = ImVec2(0, ImGui::GetFrameHeight()*gShowMenuBar);
    ImVec2 size = ImGui::GetIO().DisplaySize - menuPos;

    auto& lua = config::get_lua();
    ImRect area(menuPos, menuPos+size);
    lua["relayout"](gWindows, area);
}

void parseLayout(const std::string& str)
{
    std::vector<int> customLayout;

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
        char* s = const_cast<char*>(str.c_str());
        while (*s) {
            int n;
            if (*s == '!' || *s == '*') {
                n = -1;
                s++;
            } else {
                char* old = s;
                n = strtol(s, &s, 10);
                if (s == old) break;
            }
            customLayout.push_back(n);
            if (*s) s++;
        }
        if (!customLayout.empty()) {
            layout = "custom";
        }
    }

    auto& lua = config::get_lua();
    lua["CUSTOM_LAYOUT"] = customLayout;
    if (!layout.empty()) {
        lua["CURRENT_LAYOUT"] = layout;
    }
}

