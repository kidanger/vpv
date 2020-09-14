#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "editors.hpp"

struct View;
struct Player;
struct Colormap;
struct Image;
struct SVG;
class ImageCollection;
class EditGUI;

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;

    std::shared_ptr<ImageCollection> collection;
    std::vector<std::string> svgglobs;
    std::vector<std::vector<std::string>> svgcollection;
    std::map<std::string, std::shared_ptr<SVG>> scriptSVGs;
    bool valid;

    int loadedFrame;
    mutable float previousFactor;

    View* view;
    Player* player;
    Colormap* colormap;
    std::shared_ptr<Image> image;
    std::string error;

    std::shared_ptr<ImageCollection> uneditedCollection;
    EditGUI* editGUI;

    Sequence();
    ~Sequence();

    void loadFilenames();

    void tick();

    void autoScaleAndBias(ImVec2 p1=ImVec2(0,0), ImVec2 p2=ImVec2(0,0), float quantile=0.);
    void snapScaleAndBias();

    std::shared_ptr<Image> getCurrentImage();
    float getViewRescaleFactor() const;
    std::vector<const SVG*> getCurrentSVGs() const;

    const std::string getTitle(int ncharname=-1) const;
    void showInfo() const;

    void setEdit(const std::string& edit, EditType edittype=PLAMBDA);
    std::string getEdit();
    int getId();

    std::string getGlob() const;
    void setGlob(const std::string& glob);

    void removeCurrentFrame();

    bool putScriptSVG(const std::string& key, const std::string& buf);

    int getDesiredFrameIndex() const;

private:
};

