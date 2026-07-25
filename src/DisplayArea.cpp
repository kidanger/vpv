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
    float s = lv.scale;
    // Ask for a ring of chunks around what is strictly visible: it is what
    // makes panning smooth with a lazy source, and it costs nothing for an
    // image that is already resident.
    rect.Expand(CHUNK_SIZE * s / 2.f);
    rect.ClipWithFull(ImRect(0, 0, image.w, image.h));
    if (rect.GetWidth() <= 0 || rect.GetHeight() <= 0)
        return;

    long x0 = (long)std::floor(rect.Min.x / s / CHUNK_SIZE);
    long y0 = (long)std::floor(rect.Min.y / s / CHUNK_SIZE);
    long x1 = (long)std::ceil(rect.Max.x / s / CHUNK_SIZE);
    long y1 = (long)std::ceil(rect.Max.y / s / CHUNK_SIZE);
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
    float ccx = (rect.Min.x + rect.GetWidth() / 2.f) / s / CHUNK_SIZE - 0.5f;
    float ccy = (rect.Min.y + rect.GetHeight() / 2.f) / s / CHUNK_SIZE - 0.5f;
    std::sort(visibleChunks.begin(), visibleChunks.end(),
        [ccx, ccy](const std::pair<size_t, size_t>& a, const std::pair<size_t, size_t>& b) {
            float da = std::hypot(a.first - ccx, a.second - ccy);
            float db = std::hypot(b.first - ccx, b.second - ccy);
            return da < db;
        });
}

void DisplayArea::draw(const std::shared_ptr<Image>& image, ImVec2 pos, ImVec2 winSize,
    const Colormap& colormap, const View& view, float factor)
{
    static std::shared_ptr<Shader::Program> checkerboard = createShader(checkerboardFragment);

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

        // step 4 will pick the level from the zoom; for now there is only one
        size_t level = 0;

        this->image = image;
        computeVisibleChunks(*image, level, ImRect(mn, mx));
        texture.update(image, level, colormap.bands, visibleChunks);
    }

    // draw a checkboard pattern
    {
        ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
        userdata->shader = checkerboard;
        ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
        ImVec2 TL = pos;
        ImVec2 BR = pos + winSize;
        ImGui::GetWindowDrawList()->AddImage(nullptr, TL, BR);
        ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, nullptr);
    }

    if (!this->image) {
        return;
    }

    // display the texture
    size_t level = texture.getLevel();
    const Level& lv = this->image->getLevel(level);
    float s = lv.scale;

    static std::shared_ptr<Shader::Program> loading = createShader(loadingFragment);
    std::vector<ImRect> missing;

    ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
    userdata->shader = colormap.shader;
    userdata->scale = colormap.getScale();
    userdata->bias = colormap.getBias();
    ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
    for (const auto& coord : visibleChunks) {
        size_t cx = coord.first, cy = coord.second;
        if (cx >= lv.cw() || cy >= lv.ch())
            continue;

        // chunk extent in level pixels; the view works in level-0 pixels
        ImVec2 tl(cx * CHUNK_SIZE * s, cy * CHUNK_SIZE * s);
        ImVec2 br((cx * CHUNK_SIZE + lv.chunkWidth(cx)) * s,
            (cy * CHUNK_SIZE + lv.chunkHeight(cy)) * s);

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

        const TextureTile* t = texture.getTile(cx, cy);
        if (!t) {
            // Not read yet. Step 4 draws the co-located chunk of a coarser
            // level here instead of a spinner, when there is one.
            if (missing.size() < MAX_SPINNERS) {
                missing.push_back(ImRect(ImVec2(minX, minY), ImVec2(maxX, maxY)));
            }
            continue;
        }

        ImGui::GetWindowDrawList()->AddImageQuad((void*)(size_t)t->id, a, b, c, d);
    }
    ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, nullptr);

    if (!missing.empty()) {
        ImGui::ShaderUserData* spinner = new ImGui::ShaderUserData;
        spinner->shader = loading;
        ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, spinner);
        for (const ImRect& r : missing) {
            ImGui::GetWindowDrawList()->AddImage(nullptr, r.Min, r.Max);
        }
        ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, nullptr);
        // keep repainting: the chunks are on their way, and the spinner spins
        gActive = std::max(gActive, 2);
    }
}

ImVec2 DisplayArea::getCurrentSize() const
{
    if (image) {
        return ImVec2(image->w, image->h);
    }
    return ImVec2();
}
