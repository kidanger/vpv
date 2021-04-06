#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "EditGUI.hpp"
#include "collection_expression.hpp"
#include "editors.hpp"

struct View;
struct Player;
struct Colormap;
struct Image;
struct SVG;
class ImageCollection;
class ImageProvider;

struct Sequence {
    std::string ID;
    std::string name;

    std::shared_ptr<ImageCollection> collection;
    std::vector<std::vector<std::string>> svgcollection;
    std::map<std::string, std::shared_ptr<SVG>> scriptSVGs;
    bool valid;

    int loadedFrame;
    mutable float previousFactor;

    View* view;
    Player* player;
    Colormap* colormap;
    std::shared_ptr<ImageProvider> imageprovider;
    std::shared_ptr<Image> image;
    std::string error;

    std::shared_ptr<ImageCollection> uneditedCollection;
    EditGUI editGUI;

    Sequence();
    ~Sequence();

    void setImageCollection(std::shared_ptr<ImageCollection> imagecollection, const std::string& name="<unnamed>");
    void setSVGGlobs(const std::vector<std::string>& svgglobs);
    const std::string& getName() const;

    void tick();
    void forgetImage();

    void autoScaleAndBias(ImVec2 p1=ImVec2(0,0), ImVec2 p2=ImVec2(0,0), float quantile=0.);
    void snapScaleAndBias();

    std::shared_ptr<Image> getCurrentImage();
    float getViewRescaleFactor() const;
    std::vector<const SVG*> getCurrentSVGs() const;

    const std::string getTitle(int ncharname=-1) const;
    void showInfo() const;

    void setEdit(const std::string& edit, EditType edittype=PLAMBDA);
    std::string getEdit() const;
    int getId() const;

    void attachView(View* v);
    void attachPlayer(Player* p);
    void attachColormap(Colormap* c);

    void removeCurrentFrame();

    bool putScriptSVG(const std::string& key, const std::string& buf);

private:
    int getDesiredFrameIndex() const;
};

