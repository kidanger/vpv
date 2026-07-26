#include <algorithm>
#include <cmath>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "imgui_custom.hpp"

#include "Colormap.hpp"
#include "DisplayArea.hpp"
#include "Image.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "globals.hpp"
#include "shaders.hpp"

#define S(...) #__VA_ARGS__

static std::string checkerboardFragment = S(
    uniform vec3 scale;
    out vec4 out_color;
    void main() {
        vec2 v = floor(0.5 + gl_FragCoord.xy / 6.);
        float x = 0.12 + 0.03 * float(mod(v.x, 2.) == mod(v.y, 2.));
        out_color = vec4(x, x, x, 1.0);
    });

// Drawn over the image extent when we refuse to load anything for it. Screen
// space, like the checkerboard: it can then never be mistaken for image data,
// whatever the zoom is.
static std::string hatchFragment = S(
    uniform vec3 scale;
    out vec4 out_color;
    void main() {
        float x = mod(gl_FragCoord.x - gl_FragCoord.y, 16.) < 8. ? 0.05 : 0.09;
        out_color = vec4(x, x, x, 1.0);
    });

// Drawn in place of a chunk that has been asked for but has not arrived yet.
// Adapted from https://www.shadertoy.com/view/Xd3cR8
static std::string loadingFragment = S(
    uniform vec3 scale;
    uniform vec3 time;
    out vec4 out_color;
    in vec2 f_texcoord;

    float circle(vec2 uv, vec2 pos, float rad) {
        return 1.0 - smoothstep(rad, rad + 0.005, length(uv - pos));
    }

    float ring(vec2 uv, vec2 pos, float innerRad, float outerRad) {
        float aa = 2.0 / 200.;
        return (1.0 - smoothstep(outerRad, outerRad + aa, length(uv - pos)))
            * smoothstep(innerRad - aa, innerRad, length(uv - pos));
    }

    void main() {
        vec2 uv = f_texcoord.xy - 0.5;

        float RADIUS = 0.1;
        float THICCNESS = 0.03;
        float geo = ring(uv, vec2(0.0), RADIUS - THICCNESS, RADIUS);

        float rot = -time.x * 0.005;
        uv *= mat2(cos(rot), sin(rot), -sin(rot), cos(rot));

        float a = atan(uv.x, uv.y) * 3.14159 * 0.05 + 0.5;
        a = max(a, circle(uv, vec2(0.0, -RADIUS + THICCNESS / 2.0), THICCNESS / 2.0));
        out_color = vec4(a * geo);
    });

// Do not draw thousands of spinners; past this many missing chunks the view is
// so zoomed out that they would be a pixel each anyway.
#define MAX_SPINNERS 64

void DisplayArea::computeVisibleChunks(const Image& image, size_t level, ImRect rect)
{
    visibleChunks.clear();

    const Level& lv = image.getLevel(level);

    // level-0 pixels -> level pixels -> chunk coordinates
    double sx = lv.scaleX, sy = lv.scaleY;
    // Ask for a ring of chunks around what is strictly visible: it is what
    // makes panning smooth with a lazy source, and it costs nothing for an
    // image that is already resident.
    rect.Expand(ImVec2(CHUNK_SIZE * sx / 2.f, CHUNK_SIZE * sy / 2.f));
    rect.ClipWithFull(ImRect(0, 0, image.w, image.h));
    if (rect.GetWidth() <= 0 || rect.GetHeight() <= 0)
        return;

    long x0 = (long)std::floor(rect.Min.x / sx / CHUNK_SIZE);
    long y0 = (long)std::floor(rect.Min.y / sy / CHUNK_SIZE);
    long x1 = (long)std::ceil(rect.Max.x / sx / CHUNK_SIZE);
    long y1 = (long)std::ceil(rect.Max.y / sy / CHUNK_SIZE);
    x0 = std::max(x0, 0l);
    y0 = std::max(y0, 0l);
    x1 = std::min(x1, (long)lv.cw());
    y1 = std::min(y1, (long)lv.ch());

    for (long y = y0; y < y1; y++) {
        for (long x = x0; x < x1; x++) {
            visibleChunks.push_back({ (size_t)x, (size_t)y });
        }
    }

    // load from the centre of the view outwards: with a lazy source this is
    // what makes panning feel responsive
    float ccx = (rect.Min.x + rect.GetWidth() / 2.f) / sx / CHUNK_SIZE - 0.5f;
    float ccy = (rect.Min.y + rect.GetHeight() / 2.f) / sy / CHUNK_SIZE - 0.5f;
    std::sort(visibleChunks.begin(), visibleChunks.end(),
        [ccx, ccy](const std::pair<size_t, size_t>& a, const std::pair<size_t, size_t>& b) {
            float da = std::hypot(a.first - ccx, a.second - ccy);
            float db = std::hypot(b.first - ccx, b.second - ccy);
            return da < db;
        });
}

// Coarsest level that does not have to be magnified at 'zoomfactor' screen
// pixels per level-0 pixel.
//
// 'levels' is ordered by increasing scale, so the last level that is not
// magnified is the coarsest usable one. Biasing towards the finer level (never
// magnifying) means we always minify, which is what the GPU's filtering is good
// at, and it is why GL mipmaps are still worth generating within a level.
size_t selectLevel(const Image& image, float zoomfactor)
{
    size_t best = 0;
    for (size_t l = 1; l < image.getLevelCount(); l++) {
        if (image.getLevel(l).scale() * zoomfactor > 1.0000001)
            break;
        best = l;
    }
    return best;
}

// How many pixels of 'level' the view covers. This, not the zoom, is what the
// loading actually costs.
double sourcePixels(const Level& lv, const ImRect& rect)
{
    double w = std::min((double)rect.GetWidth(), (double)lv.w * lv.scaleX) / lv.scaleX;
    double h = std::min((double)rect.GetHeight(), (double)lv.h * lv.scaleY) / lv.scaleY;
    return std::max(w, 0.) * std::max(h, 0.);
}

bool DisplayArea::drawFallback(const ImRect& r, size_t level, const View& view, ImVec2 pos,
    ImVec2 winSize, float factor)
{
    if (!image)
        return false;

    for (size_t l = level + 1; l < image->getLevelCount(); l++) {
        const Level& lv = image->getLevel(l);
        double sx = lv.scaleX, sy = lv.scaleY;

        // The chunk grids of two levels are only nested when the scale ratio is
        // an integer, so a chunk of 'level' can straddle up to four chunks of a
        // coarser one. Draw the intersection with each of them separately: this
        // is what avoids stretching a coarse tile past its own extent.
        long cx0 = (long)std::floor(r.Min.x / sx / CHUNK_SIZE);
        long cy0 = (long)std::floor(r.Min.y / sy / CHUNK_SIZE);
        long cx1 = (long)std::ceil(r.Max.x / sx / CHUNK_SIZE);
        long cy1 = (long)std::ceil(r.Max.y / sy / CHUNK_SIZE);
        cx0 = std::max(cx0, 0l);
        cy0 = std::max(cy0, 0l);
        cx1 = std::min(cx1, (long)lv.cw());
        cy1 = std::min(cy1, (long)lv.ch());

        bool drawn = false;
        for (long cy = cy0; cy < cy1; cy++) {
            for (long cx = cx0; cx < cx1; cx++) {
                const TextureTile* t = texture.getTile(l, (size_t)cx, (size_t)cy);
                if (!t)
                    continue;

                // extent of the tile, in level-0 pixels
                double tx0 = cx * CHUNK_SIZE * sx, ty0 = cy * CHUNK_SIZE * sy;
                double tw = t->w * sx, th = t->h * sy;

                ImRect part(std::max((double)r.Min.x, tx0), std::max((double)r.Min.y, ty0),
                    std::min((double)r.Max.x, tx0 + tw), std::min((double)r.Max.y, ty0 + th));
                if (part.GetWidth() <= 0 || part.GetHeight() <= 0)
                    continue;

                ImVec2 uvmin((part.Min.x - tx0) / tw, (part.Min.y - ty0) / th);
                ImVec2 uvmax((part.Max.x - tx0) / tw, (part.Max.y - ty0) / th);

                ImVec2 a = view.image2window(part.Min, getCurrentSize(), winSize, factor) + pos;
                ImVec2 b = view.image2window(ImVec2(part.Max.x, part.Min.y), getCurrentSize(), winSize, factor) + pos;
                ImVec2 c = view.image2window(part.Max, getCurrentSize(), winSize, factor) + pos;
                ImVec2 d = view.image2window(ImVec2(part.Min.x, part.Max.y), getCurrentSize(), winSize, factor) + pos;

                ImGui::GetWindowDrawList()->AddImageQuad((void*)(size_t)t->id, a, b, c, d,
                    uvmin, ImVec2(uvmax.x, uvmin.y), uvmax, ImVec2(uvmin.x, uvmax.y));
                drawn = true;
            }
        }
        // A level that covers the rect only partially is still better than a
        // spinner, and a coarser one would draw underneath what we just drew.
        if (drawn)
            return true;
    }
    return false;
}

void DisplayArea::draw(const std::shared_ptr<Image>& image, ImVec2 pos, ImVec2 winSize,
    const Colormap& colormap, const View& view, float factor)
{
    static std::shared_ptr<Shader::Program> checkerboard = createShader(checkerboardFragment, "checkerboard");
    static std::shared_ptr<Shader::Program> hatch = createShader(hatchFragment, "hatch");
    auto dl = ImGui::GetWindowDrawList();

    // update the texture if we have an image
    if (image) {
        ImVec2 imSize(image->w, image->h);
        ImVec2 wc[4] = {
            ImVec2(0, 0), ImVec2(winSize.x, 0),
            winSize, ImVec2(0, winSize.y)
        };
        ImVec2 mn = view.window2image(wc[0], imSize, winSize, factor);
        ImVec2 mx = mn;
        for (int i = 1; i < 4; i++) {
            ImVec2 p = view.window2image(wc[i], imSize, winSize, factor);
            mn.x = std::min(mn.x, p.x);
            mn.y = std::min(mn.y, p.y);
            mx.x = std::max(mx.x, p.x);
            mx.y = std::max(mx.y, p.y);
        }
        ImRect visible(mn, mx);
        visible.ClipWithFull(ImRect(0, 0, image->w, image->h));

        size_t level = selectLevel(*image, view.zoom * factor);

        // Refuse to fault in an unbounded number of chunks. With overviews this
        // never triggers: the level picked above is already cheap. Without them
        // there is nothing coarser to fall back to, so it triggers exactly when
        // the view is zoomed out past what one screenful of reads can cover.
        double cap = (double)gMaxViewportSize * gMaxViewportSize;
        while (sourcePixels(image->getLevel(level), visible) > cap
            && level + 1 < image->getLevelCount()) {
            level++;
        }
        tooExpensive = sourcePixels(image->getLevel(level), visible) > cap;

        this->image = image;
        this->level = level;
        if (tooExpensive) {
            visibleChunks.clear();
        } else {
            computeVisibleChunks(*image, level, visible);
            texture.update(image, level, colormap.bands, visibleChunks);
        }
    }

    // draw a checkboard pattern
    {
        ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
        userdata->shader = checkerboard;
        dl->AddCallback(ImGui::SetShaderCallback, userdata);
        ImVec2 TL = pos;
        ImVec2 BR = pos + winSize;
        dl->AddImage(nullptr, TL, BR);
        dl->AddCallback(ImGui::SetShaderCallback, nullptr);
    }

    if (!this->image) {
        return;
    }

    // Nothing will be drawn for the image itself: mark its extent so the state
    // is not confused with an image that is simply dark. A chunk that is merely
    // late gets a spinner further down instead, and real pixels stay untouched.
    if (tooExpensive || visibleChunks.empty()) {
        ImVec2 imSize = getCurrentSize();
        ImVec2 a = view.image2window(ImVec2(0, 0), imSize, winSize, factor) + pos;
        ImVec2 b = view.image2window(ImVec2(imSize.x, 0), imSize, winSize, factor) + pos;
        ImVec2 c = view.image2window(imSize, imSize, winSize, factor) + pos;
        ImVec2 d = view.image2window(ImVec2(0, imSize.y), imSize, winSize, factor) + pos;

        ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
        userdata->shader = hatch;
        dl->AddCallback(ImGui::SetShaderCallback, userdata);
        dl->AddImageQuad(nullptr, a, b, c, d);
        dl->AddCallback(ImGui::SetShaderCallback, nullptr);
    }

    if (tooExpensive) {
        // The hatch says "not drawn", this says why. The actionable version of
        // the message lives in the info window.
        const char* msg = "Zoom in to display.";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        // in a small pane the label would cover everything; the hatch is enough
        if (ts.x + 8 <= winSize.x && ts.y + 8 <= winSize.y) {
            ImVec2 at = pos + (winSize - ts) / 2.f;
            dl->AddRectFilled(at - ImVec2(4, 4), at + ts + ImVec2(4, 4), IM_COL32(0, 0, 0, 160));
            dl->AddText(at, IM_COL32(230, 230, 230, 200), msg);
        }
    }

    if (visibleChunks.empty()) {
        return;
    }

    // display the texture
    const Level& lv = this->image->getLevel(this->level);
    double sx = lv.scaleX, sy = lv.scaleY;

    static std::shared_ptr<Shader::Program> loading = createShader(loadingFragment);
    std::vector<ImRect> missing;

    ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
    userdata->shader = colormap.shader;
    userdata->scale = colormap.getScale();
    userdata->bias = colormap.getBias();
    dl->AddCallback(ImGui::SetShaderCallback, userdata);
    for (const auto& coord : visibleChunks) {
        size_t cx = coord.first, cy = coord.second;
        if (cx >= lv.cw() || cy >= lv.ch())
            continue;

        // chunk extent in level pixels; the view works in level-0 pixels
        ImVec2 tl(cx * CHUNK_SIZE * sx, cy * CHUNK_SIZE * sy);
        ImVec2 br((cx * CHUNK_SIZE + lv.chunkWidth(cx)) * sx,
            (cy * CHUNK_SIZE + lv.chunkHeight(cy)) * sy);

        ImVec2 a = view.image2window(ImVec2(tl.x, tl.y), getCurrentSize(), winSize, factor) + pos;
        ImVec2 b = view.image2window(ImVec2(br.x, tl.y), getCurrentSize(), winSize, factor) + pos;
        ImVec2 c = view.image2window(ImVec2(br.x, br.y), getCurrentSize(), winSize, factor) + pos;
        ImVec2 d = view.image2window(ImVec2(tl.x, br.y), getCurrentSize(), winSize, factor) + pos;

        // AABB cull against the window rect
        float minX = std::min({ a.x, b.x, c.x, d.x });
        float maxX = std::max({ a.x, b.x, c.x, d.x });
        float minY = std::min({ a.y, b.y, c.y, d.y });
        float maxY = std::max({ a.y, b.y, c.y, d.y });
        if (minX > pos.x + winSize.x)
            continue;
        if (maxX < pos.x)
            continue;
        if (minY > pos.y + winSize.y)
            continue;
        if (maxY < pos.y)
            continue;

        const TextureTile* t = texture.getTile(this->level, cx, cy);
        if (!t) {
            // Is anything actually on its way? A chunk whose read failed for
            // good (a band that does not exist, an IO error) will never arrive,
            // and asking to repaint for it would spin at full framerate forever.
            bool pending = false;
            for (int b = 0; b < 3 && !pending; b++) {
                if (colormap.bands[b] < this->image->c)
                    pending = this->image->isChunkPending(this->level, colormap.bands[b], cx, cy);
            }

            // Not read yet: show whatever a coarser level already has. Coarser
            // chunks are never requested for this, only reused, so this only
            // helps once the view has been zoomed out at some point.
            if (!drawFallback(ImRect(tl, br), this->level, view, pos, winSize, factor)
                && pending && missing.size() < MAX_SPINNERS) {
                missing.push_back(ImRect(ImVec2(minX, minY), ImVec2(maxX, maxY)));
            }
            if (pending) {
                // the chunk is on its way; come back and draw it when it lands
                gActive = std::max(gActive, 2);
            }
            continue;
        }

        dl->AddImageQuad((void*)(size_t)t->id, a, b, c, d);
    }
    dl->AddCallback(ImGui::SetShaderCallback, nullptr);

    if (!missing.empty()) {
        ImGui::ShaderUserData* spinner = new ImGui::ShaderUserData;
        spinner->shader = loading;
        dl->AddCallback(ImGui::SetShaderCallback, spinner);
        for (const ImRect& r : missing) {
            dl->AddImage(nullptr, r.Min, r.Max);
        }
        dl->AddCallback(ImGui::SetShaderCallback, nullptr);
    }
}

ImVec2 DisplayArea::getCurrentSize() const
{
    if (image) {
        return ImVec2(image->w, image->h);
    }
    return ImVec2();
}

#include <doctest.h>

namespace {

// A source that only describes a pyramid; it never produces anything, which is
// all the level arithmetic needs.
class FakePyramidSource : public ChunkSource {
    std::vector<Level> levels;

public:
    explicit FakePyramidSource(const std::vector<Level>& levels)
        : levels(levels)
    {
    }
    std::shared_ptr<Chunk> fetch(size_t, BandIndex, size_t, size_t) override { return nullptr; }
    std::vector<Level> describeLevels() const override { return levels; }
};

}

TEST_CASE("selectLevel never magnifies")
{
    // 4000x2000 with two overviews, one of them not a power of two
    std::vector<Level> levels {
        Level(4000, 2000, 1.0, 1.0),
        Level(2000, 1000, 2.0, 2.0),
        Level(500, 250, 8.0, 8.0),
    };
    Image img(std::make_shared<FakePyramidSource>(levels), 4000, 2000, 1);
    REQUIRE(img.getLevelCount() == 3);

    CHECK(selectLevel(img, 4.f) == 0); // zoomed in
    CHECK(selectLevel(img, 1.f) == 0); // 1:1
    CHECK(selectLevel(img, 0.6f) == 0); // level 1 would be magnified
    CHECK(selectLevel(img, 0.5f) == 1); // exactly 1:1 for level 1
    CHECK(selectLevel(img, 0.2f) == 1); // level 2 would be magnified
    CHECK(selectLevel(img, 0.125f) == 2);
    CHECK(selectLevel(img, 0.01f) == 2); // nothing coarser exists

    SUBCASE("a single-level image always answers 0")
    {
        float* pixels = (float*)calloc(4, sizeof(float));
        Image plain(pixels, 2, 2, 1);
        CHECK(selectLevel(plain, 0.001f) == 0);
    }
}

TEST_CASE("sourcePixels counts pixels of the level, not of the view")
{
    Level fine(4000, 2000, 1.0, 1.0);
    Level coarse(500, 250, 8.0, 8.0);

    // the whole image
    ImRect all(0, 0, 4000, 2000);
    CHECK(sourcePixels(fine, all) == doctest::Approx(4000. * 2000));
    CHECK(sourcePixels(coarse, all) == doctest::Approx(500. * 250));

    // a viewport smaller than the image
    ImRect part(0, 0, 1024, 1024);
    CHECK(sourcePixels(fine, part) == doctest::Approx(1024. * 1024));
    CHECK(sourcePixels(coarse, part) == doctest::Approx(128. * 128));

    // clipped to the level's extent, so a rect bigger than the image does not
    // inflate the count
    ImRect huge(0, 0, 100000, 100000);
    CHECK(sourcePixels(coarse, huge) == doctest::Approx(500. * 250));
}
