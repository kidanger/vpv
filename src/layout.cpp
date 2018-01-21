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

Layout currentLayout = GRID;

std::map<Layout, std::string> layoutNames = {
    {HORIZONTAL, "horizontal"},
    {VERTICAL, "vertical"},
    {GRID, "grid"},
    {FREE, "free"},
    {FULLSCREEN, "fullscreen"},
    {CUSTOM, "custom"},
};

std::vector<int> customLayout;

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
    ImVec2 menuPos = ImVec2(0, 19*gShowMenu);
    ImVec2 size = ImGui::GetIO().DisplaySize - menuPos;

    std::vector<Window*> openedWindows;
    for (auto win : gWindows) {
        if (win->opened) {
            openedWindows.push_back(win);
        }
    }

    int num = openedWindows.size();
    if (num == 0)
        return;
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
            break;

        case CUSTOM:
            {
                // replace -1 with valid values
                auto layout = customLayout;
                int sum = 0;
                int negatives = 0;
                int last_neg = 0;
                for (int i = 0; i < layout.size(); i++) {
                    sum += std::max(0, layout[i]);
                    negatives += !!std::min(0, layout[i]);
                    last_neg = std::min(0, layout[i]) ? i : last_neg;
                }
                if (negatives) {
                    int splitted = std::max(0, (int) openedWindows.size() - sum) / negatives;
                    int rem = std::max(0, (int) openedWindows.size() - sum) % negatives;
                    for (int i = 0; i < layout.size(); i++) {
                        if (layout[i] < 0)
                            layout[i] = splitted + (last_neg == i) * rem;
                    }
                }

                int n = layout.size();
                int index = 0;

                ImVec2 _start = menuPos;
                ImVec2 _size = size;
                _size.y = (int) _size.y / n;
                ImVec2 step = ImVec2(0, _size.y);

                for (int i = 0; i < n; i++) {
                    int endindex = index + layout[i];
                    endindex = std::min(endindex, (int)openedWindows.size());

                    std::vector<Window*> wins(openedWindows.begin() + index, openedWindows.begin() + endindex);
                    if (wins.empty())
                        break;

                    ImVec2 _step = ImVec2((int) _size.x / wins.size(), 0);
                    steplayout(_start, _start + _size, _step, wins);
                    _start += step;

                    index = endindex;
                }

                for (int i = index; i < openedWindows.size(); i++) {
                    openedWindows[i]->opened = false;
                }
            }
            break;

        case FREE:
        default:
            break;
    }

    if (rezoom) {
        for (auto win : gWindows) {
            if (!win->opened) continue;
            for (auto seq : win->sequences) {
                if (!seq->valid) continue;
                seq->view->center = ImVec2(0.5f, 0.5f);
                const Image* img = seq->getCurrentImage();
                if (img && config::get_bool("AUTOZOOM")) {
                    float factor = seq->getViewRescaleFactor();
                    seq->view->setOptimalZoom(win->contentRect.GetSize(), ImVec2(img->w, img->h), factor);
                }
            }
        }
    }
}

void parseLayout(const std::string& str)
{
    customLayout.clear();
    if (str == "g" || str == "grid") {
        currentLayout = GRID;
    } else if (str == "f" || str == "fullscreen") {
        currentLayout = FULLSCREEN;
    } else if (str == "h" || str == "horizontal") {
        currentLayout = HORIZONTAL;
    } else if (str == "v" || str == "vertical") {
        currentLayout = VERTICAL;
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
        if (!customLayout.empty())
            currentLayout = CUSTOM;
    }
}

