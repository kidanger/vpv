#include <iostream>
#include <fstream>
#include <cmath>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/OpenGL.hpp>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

extern "C" {
#include "iio.h"
}

#include "globals.hpp"
#include "Window.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "Player.hpp"
#include "Colormap.hpp"
#include "Image.hpp"
#include "Shader.hpp"
#include "layout.hpp"
#include "SVG.hpp"
#include "config.hpp"

static ImRect getClipRect();

static bool file_exists(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

namespace ImGui {

static ImU32 InvertColorU32(ImU32 in)
{
    ImVec4 in4 = ColorConvertU32ToFloat4(in);
    in4.x = 1.f - in4.x;
    in4.y = 1.f - in4.y;
    in4.z = 1.f - in4.z;
    return GetColorU32(in4);
}

static float getterValue(const float *data, int id1, int id2, int stride) {
    return data[id1*stride + id2];
}

static void PlotMultiEx(
    ImGuiPlotType plot_type,
    const char* label,
    int num_datas,
    const char** names,
    const ImColor* colors,
    float(*getter)(const float* data, int idx, int idx2, int stride),
    float *datas,
    int values_count,
    float scale_min,
    float scale_max,
    ImVec2 graph_size, int stride)
{
    const int values_offset = 0;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    if (graph_size.x == 0.0f)
        graph_size.x = CalcItemWidth();
    if (graph_size.y == 0.0f)
        graph_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, NULL))
        return;

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX)
    {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int data_idx = 0; data_idx < num_datas; ++data_idx)
        {
            for (int i = 0; i < values_count; i++)
            {
                const float v = getter(datas,data_idx, i, stride);
                v_min = ImMin(v_min, v);
                v_max = ImMax(v_max, v);
            }
        }
        if (scale_min == FLT_MAX)
            scale_min = v_min;
        if (scale_max == FLT_MAX)
            scale_max = v_max;
    }
    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    int res_w = ImMin((int) graph_size.x, values_count) + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);
    int item_count = values_count + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);

    // Tooltip on hover
    int v_hovered = -1;
    if (IsHovered(inner_bb, 0))
    {
        const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
        const int v_idx = (int) (t * item_count);
        IM_ASSERT(v_idx >= 0 && v_idx < values_count);

        // std::string toolTip;
        ImGui::BeginTooltip();
        const int idx0 = (v_idx + values_offset) % values_count;
        if (plot_type == ImGuiPlotType_Lines)
        {
            const int idx1 = (v_idx + 1 + values_offset) % values_count;
            Text("%8d %8d | Name", v_idx, v_idx+1);
            for (int dataIdx = 0; dataIdx < num_datas; ++dataIdx)
            {
                const float v0 = getter(datas,dataIdx, idx0, stride);
                const float v1 = getter(datas,dataIdx, idx1, stride);
                TextColored(colors[dataIdx], "%8.4g %8.4g | %s", v0, v1, names[dataIdx]);
            }
        }
        else if (plot_type == ImGuiPlotType_Histogram)
        {
            for (int dataIdx = 0; dataIdx < num_datas; ++dataIdx)
            {
                const float v0 = getter(datas,dataIdx, idx0, stride);
                TextColored(colors[dataIdx], "%d: %8.4g | %s", v_idx, v0, names[dataIdx]);
            }
        }
        ImGui::EndTooltip();
        v_hovered = v_idx;
    }

    for (int data_idx = 0; data_idx < num_datas; ++data_idx)
    {
        const float t_step = 1.0f / (float) res_w;

        float v0 = getter(datas,data_idx, (0 + values_offset) % values_count,stride);
        float t0 = 0.0f;
        ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) / (scale_max - scale_min)));    // Point in the normalized space of our target rectangle

        const ImU32 col_base = colors[data_idx];
        const ImU32 col_hovered = InvertColorU32(colors[data_idx]);

        //const ImU32 col_base = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLines : ImGuiCol_PlotHistogram);
        //const ImU32 col_hovered = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLinesHovered : ImGuiCol_PlotHistogramHovered);
        for (int n = 0; n < res_w; n++)
        {
            const float t1 = t0 + t_step;
            const int v1_idx = (int) (t0 * item_count + 0.5f);
            IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
            const float v1 = getter(datas,data_idx, (v1_idx + values_offset + 1) % values_count, stride);
            const ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) / (scale_max - scale_min)));

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, (plot_type == ImGuiPlotType_Lines) ? tp1 : ImVec2(tp1.x, 1.0f));
            if (plot_type == ImGuiPlotType_Lines)
            {
                window->DrawList->AddLine(pos0, pos1, v_hovered == v1_idx ? col_hovered : col_base);
            }
            else if (plot_type == ImGuiPlotType_Histogram)
            {
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                window->DrawList->AddRectFilled(pos0, pos1, v_hovered == v1_idx ? col_hovered : col_base);
            }

            t0 = t1;
            tp0 = tp1;
        }
    }

    RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
}

void PlotMultiLines(
    const char* label,
    int num_datas,
    const char** names,
    const ImColor* colors,
    float(*getter)(const float* data, int idx, int idx2, int stride),
    float *datas,
    int values_count,
    float scale_min,
    float scale_max,
    ImVec2 graph_size, int stride)
{
    PlotMultiEx(ImGuiPlotType_Lines, label, num_datas, names, colors, getter, datas, values_count, scale_min, scale_max, graph_size, stride);
}

void PlotMultiHistograms(
    const char* label,
    int num_hists,
    const char** names,
    const ImColor* colors,
    float(*getter)(const float* data, int idx, int idx2, int stride),
    float *datas,
    int values_count,
    float scale_min,
    float scale_max,
    ImVec2 graph_size, int stride)
{
    PlotMultiEx(ImGuiPlotType_Histogram, label, num_hists, names, colors, getter, datas, values_count, scale_min, scale_max, graph_size, stride);
}



} // namespace ImGui

Window::Window()
{
    static int id = 0;
    id++;
    ID = "Window " + std::to_string(id);

    opened = true;
    index = 0;
}

void Window::display()
{
    screenshot = false;

    int index = std::find(gWindows.begin(), gWindows.end(), this) - gWindows.begin();
    bool isKeyFocused = !ImGui::GetIO().WantCaptureKeyboard && index <= 9
        && ImGui::IsKeyPressed(sf::Keyboard::Num1 + index) && !ImGui::IsKeyDown(sf::Keyboard::LAlt);

    if (isKeyFocused && !opened) {
        opened = true;
        relayout(false);
    }

    bool gotFocus = false;
    if (isKeyFocused || shouldAskFocus) {
        ImGui::SetNextWindowFocus();
        shouldAskFocus = false;
        gotFocus = true;
    }

    if (!opened) {
        return;
    }

    if (forceGeometry) {
        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSize(size);
        forceGeometry = false;
    }

    auto prevcolor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", getTitle().c_str(), ID.c_str());
    if (!ImGui::Begin(buf, &opened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                                    | ImGuiWindowFlags_NoCollapse | (currentLayout!=FREE?ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize:0))) {
        ImGui::End();
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = prevcolor;
        return;
    }

    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = prevcolor;

    // just closed
    if (!opened) {
        relayout(false);
    }

    if (ImGui::IsWindowFocused()) {
        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::Space)) {
            this->index = (this->index + 1) % sequences.size();
        }
        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::BackSpace)) {
            this->index = (sequences.size() + this->index - 1) % sequences.size();
        }
        if (!gotFocus && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::Tab)) {
            int d = ImGui::IsKeyDown(sf::Keyboard::LShift) ? -1 : 1;
            int nindex = (index + d + gWindows.size()) % gWindows.size();
            gWindows[nindex]->shouldAskFocus = true;
        }
    }

    Sequence* seq = getCurrentSequence();

    if (!seq) {
        ImGui::End();
        return;
    }

    displaySequence(*seq);

    ImGui::End();
}

void Window::displaySequence(Sequence& seq)
{
    Texture& texture = seq.texture;
    View* view = seq.view;

    bool focusedit = false;
    float factor = seq.getViewRescaleFactor();
    ImRect clip = getClipRect();
    ImVec2 winSize = clip.Max - clip.Min;

    // display the image
    if (seq.colormap && seq.view && seq.player) {
        ImVec2 p1 = view->window2image(ImVec2(0, 0), texture.size, winSize, factor);
        ImVec2 p2 = view->window2image(winSize, texture.size, winSize, factor);
        seq.requestTextureArea(ImRect(p1, p2));

        ImVec2 TL = view->image2window(ImVec2(0, 0), texture.size, winSize, factor);
        ImVec2 BR = view->image2window(texture.size, texture.size, winSize, factor);

        TL += clip.Min;
        BR += clip.Min;

        if (seq.image /* assumes that if we have the image, then the texture is up to date */) {
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            ImGui::GetWindowDrawList()->CmdBuffer.back().shader = &seq.colormap->shader->shader;
            ImGui::GetWindowDrawList()->CmdBuffer.back().scale = seq.colormap->getScale();
            ImGui::GetWindowDrawList()->CmdBuffer.back().bias = seq.colormap->getBias();
            ImGui::GetWindowDrawList()->AddImage((void*)(size_t)texture.id, TL, BR);
            ImGui::PopClipRect();
        }

        const SVG* svg = seq.getCurrentSVG();
        if (svg && svg->valid && gShowSVG) {
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            svg->draw(TL, seq.view->zoom);
            ImGui::PopClipRect();
        }

        contentRect = clip;

        if (gSelectionShown) {
            ImRect clip = getClipRect();
            ImVec2 from = view->image2window(gSelectionFrom, texture.size, winSize, factor);
            ImVec2 to = view->image2window(gSelectionTo, texture.size, winSize, factor);
            from += clip.Min;
            to += clip.Min;
            ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
            ImGui::GetWindowDrawList()->AddRect(from, to, green);
            char buf[2048];
            snprintf(buf, sizeof(buf), "%d %d", (int)gSelectionFrom.x, (int)gSelectionFrom.y);
            ImGui::GetWindowDrawList()->AddText(from, green, buf);
            snprintf(buf, sizeof(buf), "%d %d", (int)gSelectionTo.x, (int)gSelectionTo.y);
            ImGui::GetWindowDrawList()->AddText(to, green, buf);
        }

        ImVec2 from = view->image2window(gHoveredPixel, texture.size, winSize, factor);
        ImVec2 to = view->image2window(gHoveredPixel+ImVec2(1,1), texture.size, winSize, factor);
        from += clip.Min;
        to += clip.Min;
        ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
        if (view->zoom*factor >= 8.f) {
            ImGui::GetWindowDrawList()->AddRect(from, to, green);
        }
    }

    if (gShowHud)
        displayInfo(seq);

    if (!seq.valid || !seq.player)
        return;

    if (ImGui::IsWindowFocused()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;

        bool zooming = !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyDown(sf::Keyboard::Z);

        ImRect winclip = getClipRect();
        ImVec2 cursor = ImGui::GetMousePos() - winclip.Min;
        ImVec2 im = ImFloor(view->window2image(cursor, texture.size, winSize, factor));
        gHoveredPixel = im;

        if (zooming && ImGui::GetIO().MouseWheel != 0.f) {
            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, texture.size, winSize, factor);

            view->changeZoom(view->zoom * (1 + 0.1 * ImGui::GetIO().MouseWheel));

            ImVec2 pos2 = view->window2image(cursor, texture.size, winSize, factor);
            view->center += (pos - pos2) / texture.size;
        }
        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::I)) {
            view->changeZoom(std::pow(2, floor(log2(view->zoom) + 1)));
        }
        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::O)) {
            view->changeZoom(std::pow(2, ceil(log2(view->zoom) - 1)));
        }

        if (!ImGui::IsMouseClicked(0) && ImGui::IsMouseDown(0) && (delta.x || delta.y)) {
            ImVec2 pos = view->window2image(ImVec2(0, 0), texture.size, winSize, factor);
            ImVec2 pos2 = view->window2image(delta, texture.size, winSize, factor);
            ImVec2 diff = pos - pos2;
            view->center += diff / texture.size;
        }

        if (ImGui::IsMouseClicked(1) && ImGui::IsWindowHovered()) {
            gSelecting = true;

            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, texture.size, winSize, factor);
            gSelectionFrom = ImFloor(pos);
            gSelectionShown = true;
        }

        if (gSelecting) {
            if (ImGui::IsMouseDown(1)) {
                ImRect clip = getClipRect();
                ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
                ImVec2 pos = view->window2image(cursor, texture.size, winSize, factor);
                gSelectionTo = ImFloor(pos);
            } else if (ImGui::IsMouseReleased(1)) {
                gSelecting = false;
                if (std::hypot(gSelectionFrom.x - gSelectionTo.x, gSelectionFrom.y - gSelectionTo.y) <= 1) {
                    gSelectionShown = false;
                } else {
                    ImVec2 diff = gSelectionTo - gSelectionFrom;
                    printf("%d %d %d %d\n", (int)gSelectionFrom.x, (int)gSelectionFrom.y, (int)diff.x, (int)diff.y);
                }
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::R)) {
            view->center = ImVec2(0.5, 0.5);
            if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                view->resetZoom();
            } else {
                view->setOptimalZoom(contentRect.GetSize(), texture.getSize(), factor);
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && !zooming &&
            (ImGui::GetIO().MouseWheel || ImGui::GetIO().MouseWheelH)) {
            const Image* img = seq.getCurrentImage();
            if (ImGui::IsKeyDown(sf::Keyboard::LShift) && img) {
                seq.colormap->radius = std::max(0.f, seq.colormap->radius * (1.f + .1f * ImGui::GetIO().MouseWheel));
            } else if (img) {
                for (int i = 0; i < 3; i++) {
                    float newcenter = seq.colormap->center[i] + seq.colormap->radius * .1f * ImGui::GetIO().MouseWheel;
                    seq.colormap->center[i] = std::min(std::max(newcenter, img->min), img->max);
                }
                seq.colormap->radius = std::max(0.f, seq.colormap->radius * (1.f + .1f * ImGui::GetIO().MouseWheelH));
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && (delta.x || delta.y) && ImGui::IsKeyDown(sf::Keyboard::LShift)) {
            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, texture.size, winSize, factor);
            const Image* img = seq.getCurrentImage();
            if (img && pos.x >= 0 && pos.y >= 0 && pos.x < img->w && pos.y < img->h) {
                std::array<float,3> v{};
                int n = std::min(img->format, Image::Format::RGB);
                img->getPixelValueAt(pos.x, pos.y, &v[0], n);
                float mean = 0;
                for (int i = 0; i < n; i++) mean += v[i] / n;
                if (!std::isnan(mean) && !std::isinf(mean)) {
                    if (ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
                        seq.colormap->center = v;
                    } else {
                        for (int i = 0; i < 3; i++)
                            seq.colormap->center[i] = mean;
                    }
                }
            }
        }

        if (seq.player) {
            // TODO: this delays the actual change of frame when we press left/right
            seq.player->checkShortcuts();
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::A)) {
            if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                seq.snapScaleAndBias();
            } else if (ImGui::IsKeyDown(sf::Keyboard::LControl)) {
                ImVec2 p1 = view->window2image(ImVec2(0, 0), texture.size, winSize, factor);
                ImVec2 p2 = view->window2image(winSize, texture.size, winSize, factor);
                seq.localAutoScaleAndBias(p1, p2);
            } else if (ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
                seq.cutScaleAndBias(config::get_float("SATURATION"));
            } else {
                seq.autoScaleAndBias();
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::S)
            && !ImGui::IsKeyDown(sf::Keyboard::LControl)) {
            if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                seq.colormap->previousShader();
            } else {
                seq.colormap->nextShader();
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::E)) {
            if (!*seq.editprog) {
                int id = 0;
                while (gSequences[id] != &seq && id < gSequences.size())
                    id++;
                id++;
                sprintf(seq.editprog, "%d", id);
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
#ifdef USE_GMIC
                    seq.edittype = Sequence::EditType::GMIC;
#else
                    std::cerr << "GMIC isn't enabled, check your compilation." << std::endl;
#endif
                } else if (ImGui::IsKeyDown(sf::Keyboard::LControl)) {
#ifdef USE_OCTAVE
                    seq.edittype = Sequence::EditType::OCTAVE;
#else
                    std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
                } else {
                    seq.edittype = Sequence::EditType::PLAMBDA;
                }
            }
            focusedit = true;
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::Comma)) {
            screenshot = true;
        }
    }

    if (seq.editprog[0]) {
        if (focusedit)
            ImGui::SetKeyboardFocusHere();
        const char* name;
        switch (seq.edittype) {
            case Sequence::EditType::PLAMBDA: name = "plambda"; break;
            case Sequence::EditType::GMIC: name = "gmic"; break;
            case Sequence::EditType::OCTAVE: name = "octave"; break;
        }
        if (ImGui::InputText(name, seq.editprog, sizeof(seq.editprog),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            seq.force_reupload = true;
        }
        if (!seq.editprog[0]) {
            seq.force_reupload = true;
        }
    }
}

void Window::displayInfo(Sequence& seq)
{
    ImVec2 pos = ImGui::GetWindowPos();
    pos += ImVec2(0, 19);  // window title bar
    if (seq.editprog[0])
        pos += ImVec2(0, 20);
    ImGui::SetNextWindowPos(pos);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_ShowBorders|ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoFocusOnAppearing;

    auto prevstyle = ImGui::GetStyle();
    ImGui::GetStyle().WindowPadding = ImVec2(4, 3);
    ImGui::GetStyle().WindowRounding = 5.f;
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.5f);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###info%s", getTitle().c_str(), ID.c_str());
    ImGui::Begin(buf, NULL, flags);
    ImGui::BringBeforeParent();

    seq.showInfo();

    if (!seq.valid)
        goto end;

    ImGui::Separator();

    {
        ImVec2 im = gHoveredPixel;
        ImGui::Text("Pixel: (%d, %d)", (int)im.x, (int)im.y);

        const Image* img = seq.getCurrentImage();
        if (img && im.x >= 0 && im.y >= 0 && im.x < img->w && im.y < img->h) {
            float v[4] = {0};
            img->getPixelValueAt(im.x, im.y, v, 4);
            if (img->format == Image::R) {
                ImGui::Text("Gray: %g", v[0]);
            } else if (img->format == Image::RG) {
                ImGui::Text("Flow: %g, %g", v[0], v[1]);
            } else if (img->format == Image::RGB) {
                ImGui::Text("RGB: %g, %g, %g", v[0], v[1], v[2]);
            } else if (img->format == Image::RGBA) {
                ImGui::Text("RGBA: %g, %g, %g, %g", v[0], v[1], v[2], v[3]);
            }
        }
    }
    ImGui::Separator();
    {
        const Image* img = seq.getCurrentImage();
        const char* names[] = {"R", "G", "B"};
        const ImColor colors[] = {ImColor(255,0,0), ImColor(0,255,0), ImColor(0,0,255)};
        ImGui::PlotMultiHistograms("Histogram", img->format, names, colors, &ImGui::getterValue,
                (float*)(img->histosValues.data()),10, 0.0f, 1.0f, ImVec2(0,80), 10);
    }

    if (ImGui::IsWindowFocused() && ImGui::GetIO().MouseDoubleClicked[0]) {
        gShowHud = false;
    }

end:
    ImGui::End();
    ImGui::GetStyle() = prevstyle;
}

void Window::displaySettings()
{
    if (ImGui::Checkbox("Opened", &opened))
        relayout(false);
    ImGui::Text("Sequences");
    ImGui::SameLine(); ImGui::ShowHelpMarker("Choose which sequences are associated with this window");
    ImGui::BeginChild("scrolling", ImVec2(350, ImGui::GetItemsLineHeightWithSpacing()*3 + 20),
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto seq : gSequences) {
        auto it = std::find(sequences.begin(), sequences.end(), seq);
        bool selected = it != sequences.end();
        ImGui::PushID(seq);
        if (ImGui::Selectable(seq->glob.c_str(), selected)) {
            if (!selected) {
                sequences.push_back(seq);
            } else {
                sequences.erase(it);
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SliderInt("Index", &index, 0, sequences.size()-1);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Choose which sequence to display in the window (space / backspace)");
    index = (index + sequences.size()) % sequences.size();
}

void Window::postRender()
{
    if (!screenshot) return;

    ImVec2 winSize = ImGui::GetIO().DisplaySize;
    int x = contentRect.Min.x;
    int y = winSize.y - contentRect.Max.y - 0;
    int w = contentRect.Max.x - contentRect.Min.x;
    int h = contentRect.Max.y - contentRect.Min.y + 0;
    int size = 3 * w * h;

    float* data = new float[size];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(x, y, w, h, GL_RGB, GL_FLOAT, data);
    for (int i = 0; i < size; i++)
        data[i] *= 255.f;
    for (int y = 0; y < h/2; y++) {
        for (int i = 0; i < 3*w; i++) {
            float v = data[(h - y - 1)*(3*w) + i];
            data[(h - y - 1)*(3*w) + i] = data[y*(3*w) + i];
            data[y*(3*w) + i] = v;
        }
    }

    std::string filename_fmt = config::get_string("SCREENSHOT");
    int i = 1;
    while (true) {
        char filename[512];
        snprintf(filename, sizeof(filename), filename_fmt.c_str(), i);
        if (!file_exists(filename)) {
            iio_save_image_float_vec(filename, data, w, h, 3);
            printf("Screenshot saved to '%s'.\n", filename);
            break;
        }
        i++;
    }
    delete[] data;
}

Sequence* Window::getCurrentSequence() const
{
    if (sequences.empty())
        return nullptr;
    return sequences[index];
}

std::string Window::getTitle() const
{
    const Sequence* seq = getCurrentSequence();
    if (!seq)
        return "(no sequence associated)";
    return seq->getTitle();
}

ImRect getRenderingRect(ImVec2 texSize, ImRect* windowRect)
{
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 pos2 = pos + ImGui::GetContentRegionAvail();
    if (windowRect) {
        *windowRect = ImRect(pos, pos2);
    }

    ImVec2 diff = pos2 - pos;
    float aspect = (float) texSize.x / texSize.y;
    float nw = std::max(diff.x, diff.y * aspect);
    float nh = std::max(diff.y, diff.x / aspect);
    ImVec2 offset = ImVec2(nw - diff.x, nh - diff.y);

    pos -= offset / 2;
    pos2 += offset / 2;
    return ImRect(pos, pos2);
}

ImRect getClipRect()
{
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 pos2 = pos + ImGui::GetContentRegionAvail();
    return ImRect(pos, pos2);
}

