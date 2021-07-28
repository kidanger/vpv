#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "EditGUI.hpp"
#include "ImageRegistry.hpp"
#include "collection_expression.hpp"
#include "editors.hpp"

struct View;
struct Player;
struct Colormap;
struct Image;
struct SVG;
class ImageCollection;
class ImageProvider;

struct Sequence : std::enable_shared_from_this<Sequence> {
    std::string ID;
    std::string name;

    std::shared_ptr<ImageCollection> collection;
    std::vector<std::vector<std::string>> svgcollection;
    std::map<std::string, std::shared_ptr<SVG>> scriptSVGs;
    bool valid;

    int loadedFrame;
    mutable float previousFactor;

    std::shared_ptr<View> view;
    std::shared_ptr<Player> player;
    std::shared_ptr<Colormap> colormap;
    std::shared_ptr<Image> image;
    std::string error;

    std::shared_ptr<ImageCollection> uneditedCollection;
    EditGUI editGUI;

    moodycamel::ProducerToken token;

    Sequence();
    ~Sequence();

    void setImageCollection(std::shared_ptr<ImageCollection> imagecollection, const std::string& name = "<unnamed>");
    void setSVGGlobs(const std::vector<std::string>& svgglobs);
    const std::string& getName() const;

    void tick();
    void forgetImage();

    void autoScaleAndBias(ImVec2 p1 = ImVec2(0, 0), ImVec2 p2 = ImVec2(0, 0), float quantile = 0.);
    void snapScaleAndBias();

    std::shared_ptr<Image> getCurrentImage();
    float getViewRescaleFactor() const;
    std::vector<std::shared_ptr<SVG>> getCurrentSVGs() const;

    const std::string getTitle(int ncharname = -1) const;
    void showInfo() const;

    void setEdit(const std::string& edit, EditType edittype = PLAMBDA);
    const std::string& getEdit() const;
    int getId() const;

    void attachView(std::shared_ptr<View> v);
    void attachPlayer(std::shared_ptr<Player> p);
    void attachColormap(std::shared_ptr<Colormap> c);

    void removeCurrentFrame();

    bool putScriptSVG(const std::string& key, const std::string& buf);

    int getDesiredFrameIndex() const;
};
