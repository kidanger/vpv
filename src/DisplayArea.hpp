#pragma once

#include "Texture.hpp"

struct Image;
struct Colormap;
struct View;
struct Sequence;

class DisplayArea {
    Texture texture;

    const Image* image;
    ImRect loadedRect;

public:
    DisplayArea() : image(nullptr) {
    }

    void draw(const Image* image, ImVec2 pos, ImVec2 winSize, const Colormap* colormap, const View* view, float factor);
    void requestTextureArea(const Image* image, ImRect rect);
    ImVec2 getCurrentSize() const;

};

