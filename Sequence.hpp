#pragma once

#include <string>
#include <vector>
#include <map>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

namespace sf {
    class Texture;
}

class View;
class Player;

struct Image {
    int w, h;
    enum Type {
        UINT8,
        FLOAT,
    } type;
    enum Format {
        R=1,
        RG,
        RGB,
        RGBA,
    } format;
    void* pixels;

    ~Image() { free(pixels); }
};

struct Texture {
    uint id = -1;
    ImVec2 size;
    uint type;
    uint format;

    ~Texture();

    void create(int w, int h, uint type, uint format);
    ImVec2 getSize() { return size; }
};

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;
    std::vector<std::string> filenames;
    bool valid;
    bool visible;

    int loadedFrame;
    ImRect loadedRect;

    std::map<int, Image> pixelCache;

    Texture texture;
    View* view;
    Player* player;

    std::string shader;
    float scale; // TODO: move these to another object
    float bias;

    Sequence();

    void loadFilenames();

    void loadFrame(int frame);
    void loadTextureIfNeeded();

    void autoScaleAndBias();

    void getPixelValueAt(int x, int y, float* values, int d);

    Image* getCurrentImage();
};

