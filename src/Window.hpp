#pragma once

#include <string>
#include <vector>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "DisplayArea.hpp"

struct Sequence;

struct Window {
    std::string ID;
    std::vector<Sequence*> sequences;
    int index;

    DisplayArea displayarea;
    bool opened;
    ImVec2 position;
    ImVec2 size;
    bool forceGeometry;
    ImRect contentRect;

    bool shouldAskFocus;
    bool screenshot;

    Window();

    void display();
    void displaySequence(Sequence&);
    void displayInfo(Sequence&);
    void requestTextureArea(Sequence& seq, ImRect rect);

    void displaySettings();

    void postRender();

    Sequence* getCurrentSequence() const;
    std::string getTitle() const;
};

