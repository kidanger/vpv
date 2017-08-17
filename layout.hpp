#pragma once

#include <map>
#include <string>
#include <vector>

enum Layout {
    GRID, FULLSCREEN, HORIZONTAL, VERTICAL, CUSTOM,
    NUM_LAYOUTS, FREE
};

extern Layout currentLayout;
extern std::map<Layout, std::string> layoutNames;
extern std::vector<int> customLayout;

void relayout(bool rezoom);

void parseLayout(const std::string& str);

