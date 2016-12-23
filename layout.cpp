#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "layout.hpp"
#include "Sequence.hpp"
#include "Window.hpp"
#include "View.hpp"
#include "globals.hpp"

Layout currentLayout = GRID;

std::map<Layout, std::string> layoutNames = {
    {HORIZONTAL, "horizontal"},
    {VERTICAL, "vertical"},
    {GRID, "grid"},
    {FREE, "free"},
    {FULLSCREEN, "fullscreen"},
};

static void steplayout(ImVec2 start, ImVec2 end, ImVec2 step, const std::vector<Window*>& windows)
{
    ImVec2 individualSize;
    if (step.x) {
        individualSize = ImVec2(step.x, end.y - start.y);
    } else if (step.y) {
        individualSize = ImVec2(end.x - start.x, step.y);
    } else {
        individualSize = end - start;
    }
    for (auto w : windows) {
        w->position = start;
        w->size = individualSize;
        w->forceGeometry = true;
        w->contentRect.Min = start + ImVec2(0, 20);
        w->contentRect.Max = start + individualSize;
        start += step;
    }
}

void relayout(bool rezoom)
{
    ImVec2 menuPos = ImVec2(0, 19);
    ImVec2 size = ImGui::GetIO().DisplaySize - menuPos;

    std::vector<Window*> openedWindows;
    for (auto win : gWindows) {
        if (win->opened) {
            openedWindows.push_back(win);
        }
    }

    int num = openedWindows.size();
    switch (currentLayout) {
        case HORIZONTAL:
            steplayout(menuPos, menuPos + size, ImVec2((int)size.x / num, 0), openedWindows);
            break;

        case VERTICAL:
            steplayout(menuPos, menuPos + size, ImVec2(0, (int)size.y / num), openedWindows);
            break;

        case GRID:
            {
                int n = round(sqrtf(num));
                int index = 0;

                ImVec2 _start = menuPos;
                ImVec2 _size = size;
                _size.y = (int) _size.y / n;
                ImVec2 step = ImVec2(0, _size.y);

                for (int i = 0; i < n; i++) {
                    int endindex = index + num / n;
                    if (i == n-1)
                        endindex = num;

                    std::vector<Window*> wins(openedWindows.begin() + index, openedWindows.begin() + endindex);
                    ImVec2 _step = ImVec2((int) _size.x / wins.size(), 0);
                    steplayout(_start, _start + _size, _step, wins);
                    _start += step;

                    index = endindex;
                }
            }
            break;

        case FULLSCREEN:
            steplayout(menuPos, menuPos + size, ImVec2(), openedWindows);

        case FREE:
        default:
            break;
    }

    if (rezoom) {
        for (auto win : gWindows) {
            for (auto seq : win->sequences) {
                if (!seq->valid) continue;
                seq->view->center = seq->texture.getSize() / 2;
                seq->view->setOptimalZoom(win->contentRect.GetSize(), seq->texture.getSize());
            }
        }
    }
}

