#pragma once

#include <memory>

#include "Texture.hpp"

struct Image;
struct Colormap;
struct View;
struct Sequence;

class DisplayArea {
    Texture texture;

    std::shared_ptr<Image> image;
    ImRect loadedRect;

public:
    DisplayArea() : image(nullptr) {
    }

    void draw(const std::shared_ptr<Image>& image, ImVec2 pos,
              ImVec2 winSize, const Colormap* colormap, const View* view, float factor);
    void requestTextureArea(const std::shared_ptr<Image>& image, ImRect rect);
    ImVec2 getCurrentSize() const;

};

