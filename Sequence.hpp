#pragma once

#include <string>
#include <vector>
#include <map>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "Texture.hpp"

struct View;
struct Player;
struct Colormap;
struct Image;

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;
    std::vector<std::string> filenames;
    bool valid;
    bool force_reupload;

    int loadedFrame;
    ImRect loadedRect;

    Texture texture;
    View* view;
    Player* player;
    Colormap* colormap;
    const Image* image;

    char editprog[4096];
    enum EditType {
        PLAMBDA,
        GMIC,
        OCTAVE,
    } edittype;

    Sequence();

    void loadFilenames();

    void loadTextureIfNeeded();
    void forgetImage();
    void requestTextureArea(ImRect rect);

    void autoScaleAndBias();
    void smartAutoScaleAndBias(ImVec2 p1, ImVec2 p2);

    const Image* getCurrentImage(bool noedit=false);

    const std::string getTitle() const;
    void showInfo() const;
};

