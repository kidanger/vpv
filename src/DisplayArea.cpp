#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "imgui_custom.hpp"

#include "Sequence.hpp"
#include "Colormap.hpp"
#include "View.hpp"
#include "Image.hpp"
#include "DisplayArea.hpp"
#include "shaders.hpp"

#define S(...) #__VA_ARGS__

static std::string checkerboardFragment = S(
    uniform vec3 scale;
    out vec4 out_color;
    void main()
    {
        vec2 v = floor(0.5 + gl_FragCoord.xy / 6.);
        float x = 0.12 + 0.03 * float(mod(v.x, 2.) == mod(v.y, 2.));
        out_color = vec4(x, x, x, 1.0);
    }
);

void DisplayArea::draw(const std::shared_ptr<Image>& image, ImVec2 pos, ImVec2 winSize,
                       const Colormap &colormap, const View &view, float factor)
{
    static std::shared_ptr<Shader::Program> checkerboard = createShader(checkerboardFragment);

    // update the texture if we have an image
    if (image) {
        ImVec2 imSize(image->w, image->h);
        ImVec2 p1 = view.window2image(ImVec2(0, 0), imSize, winSize, factor);
        ImVec2 p2 = view.window2image(winSize, imSize, winSize, factor);
        requestTextureArea(image, ImRect(p1, p2), colormap.bands);
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
        ImVec2 TL = view.image2window(ImVec2(t.x, t.y), getCurrentSize(), winSize, factor);
        ImVec2 BR = view.image2window(ImVec2(t.x+t.w, t.y+t.h), getCurrentSize(), winSize, factor);

        TL += pos;
        BR += pos;

        if (TL.x > pos.x + winSize.x) continue;
        if (BR.x < pos.x) continue;
        if (TL.y > pos.y + winSize.y) continue;
        if (BR.y < pos.y) continue;

        ImGui::GetWindowDrawList()->AddImage((void*)(size_t)t.id, TL, BR);
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
        loadedRect.Expand(128);  // to avoid multiple uploads during zoom-out
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

