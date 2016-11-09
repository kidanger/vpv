#pragma once

#include <string>
#include <vector>

#include "imgui.h"

class Sequence;
class WindowMode;

struct Window {
    std::string ID;
    std::vector<Sequence*> sequences;
    WindowMode* mode;

    bool opened;
    ImVec2 position;
    ImVec2 size;
    bool forceGeometry;

    Window();

    void display();

    void displaySettings();

    void setMode(WindowMode* mode);
};

struct WindowMode {
    std::string type;

    WindowMode(std::string type) : type(type) {
    }
    virtual ~WindowMode() {}
    virtual void display(Window&) = 0;
    virtual void displaySettings(Window&) {
    }
    virtual void onAddSequence(Window&, Sequence*) {
    }
    virtual const std::string& getTitle(const Window& window) = 0;
};

struct FlipWindowMode : WindowMode {
    int index = 0;

    FlipWindowMode() : WindowMode("Flip") {
    }
    virtual ~FlipWindowMode() {}
    virtual void display(Window&);
    virtual void displaySettings(Window&);
    virtual void onAddSequence(Window&, Sequence*);
    virtual const std::string& getTitle(const Window& window);
};

