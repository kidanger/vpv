#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_custom.hpp"

#include "Sequence.hpp"
#include "Colormap.hpp"
#include "View.hpp"
#include "Image.hpp"
#include "DisplayArea.hpp"
#include "shaders.hpp"
#include "globals.hpp"

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

// from https://www.shadertoy.com/view/Xd3cR8
static std::string loadingFragment = S(
    uniform vec3 scale;
    uniform vec3 time;
    out vec4 out_color;
    in vec2 f_texcoord;

    vec2 remap(vec2 coord) {
        return coord / min(200,200);
    }

    float circle(vec2 uv, vec2 pos, float rad) {
        return 1.0 - smoothstep(rad,rad+0.005,length(uv-pos));
    }

    float ring(vec2 uv, vec2 pos, float innerRad, float outerRad) {
        float aa = 2.0 / min(200,200);
        return (1.0 - smoothstep(outerRad,outerRad+aa,length(uv-pos))) * smoothstep(innerRad-aa,innerRad,length(uv-pos));
    }

    void main()
    {
        //out_color = vec4(f_texcoord.xy, 1.0, 1.);
        vec2 uv = f_texcoord.xy;
        uv -= 0.5;

        float geo = 0.0;
        float RADIUS = 0.1;
        float THICCNESS = 0.03;
        geo += ring(uv,vec2(0.0),RADIUS-THICCNESS,RADIUS);

        float rot = -time.x * 0.005;
        uv *= mat2(cos(rot), sin(rot), -sin(rot), cos(rot));

        float a = atan(uv.x,uv.y)*3.14159*0.05 + 0.5;
        a = max(a,circle(uv,vec2(0.0,-RADIUS+THICCNESS/2.0),THICCNESS/2.0));
        out_color = vec4(a*geo);
    }
);


void DisplayArea::draw(const std::shared_ptr<Image>& image, ImVec2 pos, ImVec2 winSize,
                       const Colormap* colormap, const View* view, float factor)
{
    static Shader* checkerboardShader = createShader(checkerboardFragment);
    static Shader* loadingShader = createShader(loadingFragment);

    // draw a checkboard pattern
    {
        ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
        userdata->shader = checkerboardShader;
        ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
        ImVec2 TL = pos;
        ImVec2 BR = pos + winSize;
        ImGui::GetWindowDrawList()->AddImage(0, TL, BR);
        ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, NULL);
    }

    if (!image) {
        return;
    }

    ImVec2 imSize(image->w, image->h);
    ImVec2 p1 = view->window2image(ImVec2(0, 0), imSize, winSize, factor);
    ImVec2 p2 = view->window2image(winSize, imSize, winSize, factor);
    p1.x = std::max(std::min(p1.x, (float)image->w - 1), 0.f);
    p1.y = std::max(std::min(p1.y, (float)image->h - 1), 0.f);
    p2.x = std::max(std::min(p2.x, (float)image->w - 1), 0.f);
    p2.y = std::max(std::min(p2.y, (float)image->h - 1), 0.f);
    std::vector<std::pair<size_t, size_t>> visibility;
    for (size_t y = floor(p1.y/CHUNK_SIZE); y < ceil(p2.y/CHUNK_SIZE); y++) {
        for (size_t x = floor(p1.x/CHUNK_SIZE); x < ceil(p2.x/CHUNK_SIZE); x++) {
            visibility.push_back(std::make_pair(x, y));
        }
    }

    texture.upload(image, colormap->bands, visibility);
    this->image = image;  // just for getCurrentSize...

    // display the texture
    for (auto p : visibility) {
        size_t x = p.first;
        size_t y = p.second;
        size_t xx = x * CHUNK_SIZE;
        size_t yy = y * CHUNK_SIZE;
        nonstd::optional<TextureTile>& t = texture.tiles[x][y];

        ImVec2 TL = view->image2window(ImVec2(xx, yy), getCurrentSize(), winSize, factor);
        ImVec2 BR = view->image2window(ImVec2(xx+CHUNK_SIZE, yy+CHUNK_SIZE), getCurrentSize(), winSize, factor);
        TL += pos;
        BR += pos;

        if (t) {
            BR = view->image2window(ImVec2(xx+t->w, yy+t->h), getCurrentSize(), winSize, factor);
            BR += pos;

            ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
            userdata->shader = colormap->shader;
            userdata->scale = colormap->getScale();
            userdata->bias = colormap->getBias();
            ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
            ImGui::GetWindowDrawList()->AddImage((void*)(size_t)t->id, TL, BR);
        } else if (visibility.size() < 100 * 100) {
            ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
            userdata->shader = loadingShader;
            ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
            ImGui::GetWindowDrawList()->AddImage((void*)(size_t)0, TL, BR);
            gActive = 2;  // keep the animation running..
        }
    }
    ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, NULL);
}

ImVec2 DisplayArea::getCurrentSize() const
{
    if (image) {
        return ImVec2(image->w, image->h);
    }
    return ImVec2();
}

