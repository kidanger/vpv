#include <iostream>
#include <fstream>
#include <cmath>

#ifndef SDL
#include <SFML/OpenGL.hpp>
#else
#include <GL/gl3w.h>
#endif

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
#include "events.hpp"
#include "imgui_custom.hpp"

static ImRect getClipRect();

static bool file_exists(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

Window::Window()
{
    static int id = 0;
    id++;
    ID = "Window " + std::to_string(id);

    opened = true;
    index = 0;
    shouldAskFocus = false;
    screenshot = false;
}

void Window::display()
{
    screenshot = false;

    int index = std::find(gWindows.begin(), gWindows.end(), this) - gWindows.begin();
    char d[2] = {static_cast<char>('1' + index), 0};
    bool isKeyFocused = index <= 9 && isKeyPressed(d) && !isKeyDown("alt") && !isKeyDown("s");

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

    auto prevStyle = ImGui::GetStyle();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    ImGui::GetStyle().WindowPadding = ImVec2(1, 1);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", getTitle().c_str(), ID.c_str());
    int flags = ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse
                | ImGuiWindowFlags_NoFocusOnAppearing
                | ImGuiWindowFlags_NoCollapse
                | (getLayoutName()!="free" ? ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize : 0);
    if (!gShowWindowBar) {
        flags |= ImGuiWindowFlags_NoTitleBar;
    }
    if (!ImGui::Begin(buf, &opened, flags)) {
        ImGui::End();
        ImGui::GetStyle() = prevStyle;
        return;
    }

    ImGui::GetStyle() = prevStyle;

    // just closed
    if (!opened) {
        relayout(false);
    }

    if (ImGui::IsWindowFocused()) {
        if (isKeyPressed(" ")) {
            this->index = (this->index + 1) % sequences.size();
        }
        if (isKeyPressed("\b")) {
            this->index = (sequences.size() + this->index - 1) % sequences.size();
        }
        if (!gotFocus && isKeyPressed("\t")) {
            int d = isKeyDown("shift") ? -1 : 1;
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

        if (gShowImage && seq.colormap->shader) {
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            ImGui::ShaderUserData* userdata = new ImGui::ShaderUserData;
            userdata->shader = seq.colormap->shader;
            userdata->scale = seq.colormap->getScale();
            userdata->bias = seq.colormap->getBias();
            ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, userdata);
            for (auto t : texture.tiles) {
                ImVec2 TL = view->image2window(ImVec2(t.x, t.y), texture.size, winSize, factor);
                ImVec2 BR = view->image2window(ImVec2(t.x+t.w, t.y+t.h), texture.size, winSize, factor);

                TL += clip.Min;
                BR += clip.Min;

                ImGui::GetWindowDrawList()->AddImage((void*)(size_t)t.id, TL, BR);
            }
            ImGui::GetWindowDrawList()->AddCallback(ImGui::SetShaderCallback, NULL);
            ImGui::PopClipRect();
        }

        std::vector<const SVG*> svgs = seq.getCurrentSVGs();
        if (!svgs.empty()) {
            ImVec2 TL = view->image2window(seq.view->svgOffset, texture.size, winSize, factor) + clip.Min;
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            for (int i = 0; i < svgs.size(); i++) {
                if (svgs[i] && (i >= 9 || gShowSVGs[i]))
                    svgs[i]->draw(TL, seq.view->zoom*factor);
            }
            ImGui::PopClipRect();
        }

        contentRect = clip;

        if (gSelectionShown) {
            ImRect clip = getClipRect();
            ImVec2 off1, off2;
            if (gSelectionFrom.x <= gSelectionTo.x)
                off2.x = 1;
            else
                off1.x = 1;
            if (gSelectionFrom.y <= gSelectionTo.y)
                off2.y = 1;
            else
                off1.y = 1;
            ImVec2 from = view->image2window(gSelectionFrom+off1, texture.size, winSize, factor);
            ImVec2 to = view->image2window(gSelectionTo+off2, texture.size, winSize, factor);
            from += clip.Min;
            to += clip.Min;
            ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
            ImGui::GetWindowDrawList()->AddRect(from, to, green);
            char buf[2048];
            snprintf(buf, sizeof(buf), "%d %d", (int)gSelectionFrom.x, (int)gSelectionFrom.y);
            ImGui::GetWindowDrawList()->AddText(from, green, buf);
            snprintf(buf, sizeof(buf), "%d %d (d=%.2f)", (int)gSelectionTo.x, (int)gSelectionTo.y,
                     std::sqrt(ImLengthSqr(gSelectionTo - gSelectionFrom)));
            ImGui::GetWindowDrawList()->AddText(to, green, buf);
        }

        ImVec2 from = view->image2window(gHoveredPixel, texture.size, winSize, factor);
        ImVec2 to = view->image2window(gHoveredPixel+ImVec2(1,1), texture.size, winSize, factor);
        from += clip.Min;
        to += clip.Min;
        if (view->zoom*factor >= gDisplaySquareZoom) {
            ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
            ImU32 black = ImGui::GetColorU32(ImVec4(0,0,0,1));
            ImGui::GetWindowDrawList()->AddRect(from, to, black, 0, ~0, 2.5f);
            ImGui::GetWindowDrawList()->AddRect(from, to, green);
        }
    }

    static bool showthings = false;
    if (ImGui::IsWindowFocused() && isKeyPressed("F6")) {
        showthings = !showthings;
    }
    if (showthings) {
        ImVec2 size = ImGui::GetWindowSize();
        ImGui::BeginChildFrame(ImGui::GetID(".."), ImVec2(0, size.y * 0.25));
        for (int i = 0; i < sequences.size(); i++) {
            const Sequence* seq = sequences[i];
            bool flags = index == i ? ImGuiTreeNodeFlags_DefaultOpen : 0;
            if (ImGui::CollapsingHeader(seq->glob.c_str(), flags)) {
                int frame = seq->player->frame - 1;
                for (int f = 0; f < seq->collection->getLength(); f++) {
                    const std::string& filename = seq->collection->getFilename(f);
                    bool current = f == frame;
                    if (ImGui::Selectable(filename.c_str(), current)) {
                        seq->player->frame = f + 1;
                    }
                }
            }
        }
        ImGui::EndChildFrame();
    }

    if (gShowHud)
        displayInfo(seq);

    if (!seq.valid || !seq.player)
        return;

    auto f = config::get_lua()["on_window_tick"];
    if (f) {
        f(*this, ImGui::IsWindowFocused());
    }

    if (ImGui::IsWindowFocused()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;

        bool zooming = isKeyDown("z");

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
        if (isKeyPressed("i")) {
            view->changeZoom(std::pow(2, floor(log2(view->zoom) + 1)));
        }
        if (isKeyPressed("o")) {
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

        if (isKeyPressed("r")) {
            view->center = ImVec2(0.5, 0.5);
            if (isKeyDown("shift")) {
                view->resetZoom();
            } else {
                view->setOptimalZoom(contentRect.GetSize(), texture.getSize(), factor);
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && !zooming &&
            (ImGui::GetIO().MouseWheel || ImGui::GetIO().MouseWheelH)) {
            static float f = 0.1f;
            const Image* img = seq.getCurrentImage();
            if (isKeyDown("shift") && img) {
                seq.colormap->radius = std::max(0.f, seq.colormap->radius * (1.f + f * ImGui::GetIO().MouseWheel));
            } else if (img) {
                for (int i = 0; i < 3; i++) {
                    float newcenter = seq.colormap->center[i] + seq.colormap->radius * f * ImGui::GetIO().MouseWheel;
                    seq.colormap->center[i] = std::min(std::max(newcenter, img->min), img->max);
                }
                seq.colormap->radius = std::max(0.f, seq.colormap->radius * (1.f + f * ImGui::GetIO().MouseWheelH));
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && (delta.x || delta.y) && isKeyDown("shift")) {
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
                    if (isKeyDown("alt")) {
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

        if (isKeyPressed("a")) {
            if (isKeyDown("shift")) {
                seq.snapScaleAndBias();
            } else if (isKeyDown("control")) {
                ImVec2 p1 = view->window2image(ImVec2(0, 0), texture.size, winSize, factor);
                ImVec2 p2 = view->window2image(winSize, texture.size, winSize, factor);
                seq.localAutoScaleAndBias(p1, p2);
            } else if (isKeyDown("alt")) {
                seq.cutScaleAndBias(config::get_float("SATURATION"));
            } else {
                seq.autoScaleAndBias();
            }
        }

        if (isKeyPressed("s") && !isKeyDown("control")) {
            if (isKeyDown("shift")) {
                seq.colormap->previousShader();
            } else {
                seq.colormap->nextShader();
            }
        }

        if (isKeyPressed("e")) {
            if (!*seq.editprog) {
                int id = seq.getId();
                sprintf(seq.editprog, "%d", id);
                if (isKeyDown("shift")) {
#ifdef USE_GMIC
                    seq.edittype = EditType::GMIC;
#else
                    std::cerr << "GMIC isn't enabled, check your compilation." << std::endl;
#endif
                } else if (isKeyDown("control")) {
#ifdef USE_OCTAVE
                    seq.edittype = EditType::OCTAVE;
#else
                    std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
                } else {
                    seq.edittype = EditType::PLAMBDA;
                }
            }
            focusedit = true;
        }

        if (isKeyPressed(",")) {
            screenshot = true;
        }
    }

    if (seq.editprog[0] && !screenshot) {
        if (focusedit)
            ImGui::SetKeyboardFocusHere();
        const char* name;
        switch (seq.edittype) {
            case EditType::PLAMBDA: name = "plambda"; break;
            case EditType::GMIC: name = "gmic"; break;
            case EditType::OCTAVE: name = "octave"; break;
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
    if (gShowWindowBar)
        pos += ImVec2(0, ImGui::GetFrameHeight());
    if (seq.editprog[0])
        pos.y += ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(pos);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoFocusOnAppearing;

    auto prevstyle = ImGui::GetStyle();
    ImGui::GetStyle().WindowPadding = ImVec2(4, 3);
    ImGui::GetStyle().WindowRounding = 5.f;
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.5f);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###info%s", getTitle().c_str(), ID.c_str());
    ImGuiWindow* parent = ImGui::GetCurrentContext()->CurrentWindow;
    ImGui::Begin(buf, NULL, flags);
    ImGui::BringBefore(parent);

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
        } else {
            ImGui::Text("");
        }
    }

    if (gShowHistogram) {
        float cmin, cmax;
        seq.colormap->getRange(cmin, cmax, 3);

        const Image* img = seq.getCurrentImage();
        ((Image*) img)->computeHistogram(cmin, cmax);
        const std::vector<std::vector<long>> hist = img->histograms;

        const void* values[4];
        for (int d = 0; d < img->format; d++) {
            values[d] = hist[d].data();
        }

        const char* names[] = {"r", "g", "b", ""};
        ImColor colors[] = {
            ImColor(255, 0, 0), ImColor(0, 255, 0), ImColor(0, 0, 255), ImColor(100, 100, 100)
        };
        if (img->format == 1) {
            colors[0] = ImColor(255, 255, 255);
            names[0] = "";
        }
        auto getter = [](const void *data, int idx) {
            const long* hist = (const long*) data;
            return (float)hist[idx];
        };

        ImGui::Separator();
        ImGui::PlotMultiHistograms("", hist.size(), names, colors, getter, values,
                                   hist[0].size(), FLT_MIN, FLT_MAX, ImVec2(hist[0].size(), 80));
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
    if (sequences.size() > 0)
        this->index = (this->index + sequences.size()) % sequences.size();
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
            iio_write_image_float_vec(filename, data, w, h, 3);
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

