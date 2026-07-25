#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Colormap.hpp"
#include "Texture.hpp"

struct Image;
struct Colormap;
struct View;
struct Sequence;

class DisplayArea {
    Texture texture;

    std::shared_ptr<Image> image;
    std::vector<std::pair<size_t, size_t>> visibleChunks;

public:
    DisplayArea()
        : image(nullptr)
    {
    }

    void draw(const std::shared_ptr<Image>& image, ImVec2 pos,
        ImVec2 winSize, const Colormap& colormap, const View& view, float factor);
    ImVec2 getCurrentSize() const;

private:
    // Chunks of 'level' whose extent intersects 'rect' (in level-0 pixels),
    // ordered from the centre of the rect outwards.
    void computeVisibleChunks(const Image& image, size_t level, ImRect rect);
};
