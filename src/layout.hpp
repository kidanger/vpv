#pragma once

#include <map>
#include <string>
#include <vector>

void nextLayout();
void previousLayout();
void freeLayout();
std::string getLayoutName();

void relayout(bool rezoom);

void parseLayout(const std::string& str);

