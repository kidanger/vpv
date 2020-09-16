#pragma once

#include <memory>

#include "Colormap.hpp"
#include "Texture.hpp"

struct Image;
struct Colormap;
struct View;
struct Sequence;

class DisplayArea {
    Texture texture;
    std::shared_ptr<Image> image;

public:
    DisplayArea() {
    }

    void draw(const std::shared_ptr<Image>& image, ImVec2 pos,
              ImVec2 winSize, const Colormap* colormap, const View* view, float factor);
    ImVec2 getCurrentSize() const;

};

