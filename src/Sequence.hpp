#pragma once

#include <string>
#include <vector>
#include <map>
#include <future>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "Texture.hpp"
#include "editors.hpp"

struct View;
struct Player;
struct Colormap;
struct Image;
struct SVG;
class ImageCollection;

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;

    ImageCollection* collection;
    std::vector<std::string> svgglobs;
    std::vector<std::vector<std::string>> svgcollection;
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
    std::future<void> future;

    EditType edittype;
    char editprog[4096];

    Sequence();
    ~Sequence();

    void loadFilenames();

    void loadTextureIfNeeded();
    void forgetImage();
    void requestTextureArea(ImRect rect);

    void autoScaleAndBias();
    void snapScaleAndBias();
    void localAutoScaleAndBias(ImVec2 p1, ImVec2 p2);
    void cutScaleAndBias(float percentile);

    const Image* getCurrentImage(bool noedit=false, bool force=false);
    float getViewRescaleFactor() const;
    std::vector<const SVG*> getCurrentSVGs() const;

    const std::string getTitle() const;
    void showInfo() const;

    void setEdit(const std::string& edit, EditType edittype=PLAMBDA);
    std::string getEdit();
    int getId();
};

