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
struct SVG;

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;
    std::vector<std::string> filenames;
    std::string svgglob;
    std::vector<std::string> svgfilenames;
    bool valid;
    bool force_reupload;

    int loadedFrame;
    ImRect loadedRect;
    mutable float previousFactor;

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
    void snapScaleAndBias();
    void localAutoScaleAndBias(ImVec2 p1, ImVec2 p2);
    void cutScaleAndBias(float percentile);

    const Image* getCurrentImage(bool noedit=false);
    float getViewRescaleFactor() const;
    const SVG* getCurrentSVG() const;

    const std::string getTitle() const;
    void showInfo() const;
};

