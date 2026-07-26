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
struct Level;

// Coarsest level of 'image' that does not have to be magnified at 'zoomfactor'
// screen pixels per level-0 pixel.
size_t selectLevel(const Image& image, float zoomfactor);

// How many pixels of 'lv' the level-0 rect 'rect' covers.
double sourcePixels(const Level& lv, const ImRect& rect);

class DisplayArea {
    Texture texture;

    std::shared_ptr<Image> image;
    std::vector<std::pair<size_t, size_t>> visibleChunks;
    size_t level = 0;
    bool tooExpensive = false;

public:
    DisplayArea()
        : image(nullptr)
    {
    }

    void draw(const std::shared_ptr<Image>& image, ImVec2 pos,
        ImVec2 winSize, const Colormap& colormap, const View& view, float factor);
    ImVec2 getCurrentSize() const;

    // Draws the coarsest pyramid level of 'image' into the screen rect 'dst',
    // for the miniview, faded by 'alpha'. Only an image that has overviews gets
    // one. Returns false when nothing was drawn (no overviews, or no coarse
    // chunk on the GPU yet), in which case the caller keeps its plain
    // background. The chunks are requested but never waited for.
    bool drawThumbnail(const std::shared_ptr<Image>& image, const ImRect& dst,
        const Colormap& colormap, float alpha);

    // Pyramid level currently being displayed, and whether we gave up on
    // drawing at all because the view covers too much of the image (see
    // gMaxViewportSize). Both are only interesting for the HUD.
    size_t getLevel() const { return level; }
    bool isTooExpensive() const { return tooExpensive; }

private:
    // Chunks of 'level' whose extent intersects 'rect' (in level-0 pixels),
    // ordered from the centre of the rect outwards.
    void computeVisibleChunks(const Image& image, size_t level, ImRect rect);

    // Draws whatever any coarser level already has on the GPU for the level-0
    // rect 'r', which is a chunk of 'level' that has not been read yet. Returns
    // false when nothing was found, in which case the caller draws a spinner.
    bool drawFallback(const ImRect& r, size_t level, const View& view, ImVec2 pos,
        ImVec2 winSize, float factor);
};
