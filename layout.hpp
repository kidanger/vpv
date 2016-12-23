#pragma once

#include <map>
#include <string>

enum Layout {
    GRID, FULLSCREEN, HORIZONTAL, VERTICAL,
    NUM_LAYOUTS, FREE
};

extern Layout currentLayout;
extern std::map<Layout, std::string> layoutNames;

void relayout(bool rezoom);

