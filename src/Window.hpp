#pragma once

#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "DisplayArea.hpp"
#ifdef USE_FUZZY_FINDER
#include "FuzzyFinder.hpp"
#endif

struct Sequence;
class Histogram;

struct Window : std::enable_shared_from_this<Window> {
    std::string ID;
    std::vector<std::shared_ptr<Sequence>> sequences;
    std::shared_ptr<Histogram> histogram;
#ifdef USE_FUZZY_FINDER
    FuzzyFinderForSequence fuzzyfinder;
#endif
    int index;

    DisplayArea displayarea;
    bool opened;
    ImVec2 position;
    ImVec2 size;
    bool forceGeometry;
    ImRect contentRect;
    bool dontLayout;
    bool alwaysOnTop;

    bool shouldAskFocus;
    bool screenshot;

    // A right click that started over the miniview pans the view instead of
    // starting a selection, and keeps panning until the button is released,
    // even once the cursor has left the miniview.
    bool miniviewPanning = false;

    Window();

    void display();
    void displaySequence(Sequence&);
    void displayInfo(Sequence&);

    void displaySettings();

    void postRender();

    std::shared_ptr<Sequence> getCurrentSequence() const;
    std::string getTitle() const;
};
