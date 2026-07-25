#include <algorithm>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "imgui_custom.hpp"

#include "Colormap.hpp"
#include "DisplayArea.hpp"
#include "Image.hpp"
#include "Sequence.hpp"
#include "View.hpp"
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

void DisplayArea::draw(const std::shared_ptr<Image>& image, ImVec2 pos, ImVec2 winSize,
    const Colormap& colormap, const View& view, float factor)
{
    static std::shared_ptr<Shader::Program> checkerboard = createShader(checkerboardFragment);

    // update the texture if we have an image
    if (image) {
        ImVec2 imSize(image->w, image->h);
        ImVec2 wc[4] = {
            ImVec2(0, 0),
            ImVec2(winSize.x, 0),
            winSize,
            ImVec2(0, winSize.y)
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
        requestTextureArea(image, ImRect(mn, mx), colormap.bands);
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

    // display the texture
    ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
    userdata->shader = colormap.shader;
    userdata->scale = colormap.getScale();
    userdata->bias = colormap.getBias();
    ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
    for (auto t : texture.tiles) {
        ImVec2 a = view.image2window(ImVec2(t.x, t.y), getCurrentSize(), winSize, factor) + pos;
        ImVec2 b = view.image2window(ImVec2(t.x + t.w, t.y), getCurrentSize(), winSize, factor) + pos;
        ImVec2 c = view.image2window(ImVec2(t.x + t.w, t.y + t.h), getCurrentSize(), winSize, factor) + pos;
        ImVec2 d = view.image2window(ImVec2(t.x, t.y + t.h), getCurrentSize(), winSize, factor) + pos;

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

        ImGui::GetWindowDrawList()->AddImageQuad((void*)(size_t)t.id, a, b, c, d);
    }
    ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, nullptr);
}

void DisplayArea::requestTextureArea(const std::shared_ptr<Image>& image, ImRect rect, BandIndices bandidx)
{
    rect.Expand(1.0f);
    rect.Floor();
    rect.ClipWithFull(ImRect(0, 0, image->w, image->h));

    bool reupload = false;

    if (this->image != image) {
        this->image = image;
        loadedRect = ImRect();
        reupload = true;
    }

    if (!loadedRect.Contains(rect)) {
        loadedRect.Add(rect);
        loadedRect.Expand(128); // to avoid multiple uploads during zoom-out
        loadedRect.ClipWithFull(ImRect(0, 0, image->w, image->h));
        reupload = true;
    }

    if (loadedBands != bandidx) {
        loadedBands = bandidx;
        reupload = true;
    }

    if (reupload) {
        texture.upload(*image, loadedRect, loadedBands);
    }
}

ImVec2 DisplayArea::getCurrentSize() const
{
    if (image) {
        return ImVec2(image->w, image->h);
    }
    return ImVec2();
}
