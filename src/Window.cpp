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
#include "ImageProvider.hpp"
#include "ImageCollection.hpp"
#include "Shader.hpp"
#include "layout.hpp"
#include "SVG.hpp"
#include "Histogram.hpp"
#include "EditGUI.hpp"
#include "config.hpp"
#include "events.hpp"
#include "imgui_custom.hpp"

static ImRect getClipRect();

static bool file_exists(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

ImVec4 getNthColor(int n, float alpha=1.0)
{
    static ImVec4 colors[] = {
        ImVec4(0, 0, 1, 0),
        ImVec4(0, 1, 0, 0),
        ImVec4(1, 0, 0, 0),
        ImVec4(1, 1, 0, 0),
        ImVec4(0, 1, 1, 0),
        ImVec4(1, 0, 1, 0),
    };
    int ncolors = sizeof(colors)/sizeof(*colors);
    ImVec4 c = colors[n%ncolors];
    c.x *= 0.6f;
    c.y *= 0.6f;
    c.z *= 0.6f;
    c.x += 0.4f;
    c.y += 0.4f;
    c.z += 0.4f;
    c.w = alpha;
    return c;
}

template <typename T, typename F>
static void showTag(T*& current, std::vector<T*> all, const char* name,
                    F onNew)
{
    int idx;
    int len;
    for (idx = 0, len = all.size(); idx < len; idx++) {
        if (current == all[idx])
            break;
    }
    ImVec4 c = getNthColor(idx % len, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, c);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
    ImGui::Button(name);
    ImGui::PopStyleColor(4);
}

static void viewTable()
{
    ImGui::Columns(1+gViews.size(), "views", false);
    ImGui::Separator();
    ImGui::Text("Sequence"); ImGui::NextColumn();
    int i = 0;
    for (View* view : gViews) {
        //ImGui::TextColored(getNthColor(i, 1.0), "View");
        ImGui::SetColumnWidth(-1, 30);
        ImGui::NextColumn();
        i++;
    }
    ImGui::Separator();
    for (Sequence* seq : gSequences) {
        ImGui::Text("%s", seq->getTitle().c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::Text("%s", seq->getTitle().c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::NextColumn();
        int i = 0;
        for (View* view : gViews) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, getNthColor(i, 0.2));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, getNthColor(i, 0.6));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, getNthColor(i, 0.8));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0,0,0,0.7));
            std::string id = seq->ID + "_" + view->ID;
            if (ImGui::RadioButton(("##"+id).c_str(), seq->view == view)) {
                seq->view = view;
            }
            ImGui::PopStyleColor(4);
            ImGui::NextColumn();
            i++;
        }
    }
    ImGui::Separator();
    ImGui::NextColumn();
    int del = -1;
    for (int i = 0; i < gViews.size(); i++) {
        View* view = gViews[i];
        if (ImGui::Selectable(("delete##"+view->ID).c_str(),
                              false, ImGuiSelectableFlags_DontClosePopups)) {
            del = i;
        }
        ImGui::NextColumn();
    }
    if (del >= 0 && gViews.size() > 1) {
        View* view = gViews[del];
        for (Sequence* seq : gSequences) {
            if (seq->view == view) {
                seq->view = gViews[!del];
            }
        }
        gViews.erase(gViews.begin() + del);
        delete view;
    }
    ImGui::Columns(1);
    ImGui::Separator();
    if (ImGui::Button("New view")) {
        newView();
    }
}

static void colormapTable()
{
    ImGui::Columns(1+gColormaps.size(), "colormaps", false);
    ImGui::Separator();
    ImGui::Text("Sequence"); ImGui::NextColumn();
    int i = 0;
    for (Colormap* colormap : gColormaps) {
        ImGui::TextColored(getNthColor(i, 1.0), "Colormap");
        ImGui::NextColumn();
        i++;
    }
    ImGui::Separator();
    for (Sequence* seq : gSequences) {
        ImGui::Text("%s", seq->getTitle().c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::Text("%s", seq->getTitle().c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::NextColumn();
        for (Colormap* colormap : gColormaps) {
            std::string id = seq->ID + "_" + colormap->ID;
            if (ImGui::RadioButton(("##"+id).c_str(), seq->colormap == colormap)) {
                seq->colormap = colormap;
            }
            ImGui::NextColumn();
        }
    }
    ImGui::Separator();
    ImGui::Text("%s", "Delete");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("%s", "Delete a colormap by clicking on the cell");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::NextColumn();
    int del = -1;
    for (int i = 0; i < gColormaps.size(); i++) {
        Colormap* colormap = gColormaps[i];
        if (ImGui::Selectable(("##"+colormap->ID).c_str(),
                              false, ImGuiSelectableFlags_DontClosePopups)) {
            del = i;
        }
        ImGui::NextColumn();
    }
    if (del >= 0 && gColormaps.size() > 1) {
        Colormap* colormap = gColormaps[del];
        for (Sequence* seq : gSequences) {
            if (seq->colormap == colormap) {
                seq->colormap = gColormaps[!del];
            }
        }
        gColormaps.erase(gColormaps.begin() + del);
        delete colormap;
    }
    ImGui::Columns(1);
    ImGui::Separator();
    if (ImGui::Button("New colormap")) {
        newColormap();
    }
}

static void playerTable()
{
    ImGui::Columns(1+gPlayers.size(), "players", false);
    ImGui::Separator();
    ImGui::Text("Sequence"); ImGui::NextColumn();
    int i = 0;
    for (Player* player : gPlayers) {
        ImGui::TextColored(getNthColor(i, 1.0), "Player");
        ImGui::NextColumn();
        i++;
    }
    ImGui::Separator();
    for (Sequence* seq : gSequences) {
        ImGui::Text("%s", seq->getTitle().c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::Text("%s", seq->getTitle().c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::NextColumn();
        for (Player* player : gPlayers) {
            std::string id = seq->ID + "_" + player->ID;
            if (ImGui::RadioButton(("##"+id).c_str(), seq->player == player)) {
                seq->player = player;
                for (Player* p : gPlayers) {
                    p->reconfigureBounds();
                }
            }
            ImGui::NextColumn();
        }
    }
    ImGui::Separator();
    ImGui::Text("%s", "Delete");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("%s", "Delete a player by clicking on the cell");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::NextColumn();
    int del = -1;
    for (int i = 0; i < gPlayers.size(); i++) {
        Player* player = gPlayers[i];
        if (ImGui::Selectable(("##"+player->ID).c_str(),
                              false, ImGuiSelectableFlags_DontClosePopups)) {
            del = i;
        }
        ImGui::NextColumn();
    }
    if (del >= 0 && gPlayers.size() > 1) {
        Player* player = gPlayers[del];
        for (Sequence* seq : gSequences) {
            if (seq->player == player) {
                seq->player = gPlayers[!del];
                for (Player* p : gPlayers) {
                    p->reconfigureBounds();
                }
            }
        }
        gPlayers.erase(gPlayers.begin() + del);
        delete player;
    }
    ImGui::Columns(1);
    ImGui::Separator();
    if (ImGui::Button("New player")) {
        newPlayer();
    }
}

Window::Window()
{
    static int id = 0;
    id++;
    ID = "Window " + std::to_string(id);

    histogram = std::make_shared<Histogram>();

    opened = true;
    index = 0;
    shouldAskFocus = false;
    screenshot = false;
    dontLayout = false;
    alwaysOnTop = false;
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
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.0, 0.0, 0.0, 1.00f);
    ImGui::GetStyle().WindowPadding = ImVec2(gWindowBorder,gWindowBorder);
    ImGui::GetStyle().WindowBorderSize = gWindowBorder;

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", getTitle().c_str(), ID.c_str());
    int flags = ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse
                | ImGuiWindowFlags_NoFocusOnAppearing
                | ImGuiWindowFlags_NoCollapse
                | (getLayoutName()!="free" ? ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize : 0);
    if (gShowWindowBar == 0 || (gShowWindowBar == 2 && gWindows.size() == 1)) {
        flags |= ImGuiWindowFlags_NoTitleBar;
    }
    if (!ImGui::Begin(buf, nullptr, flags)) {
        ImGui::End();
        ImGui::GetStyle() = prevStyle;
        return;
    }

    if (alwaysOnTop) {
        ImGui::BringFront();
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

    {
        ImRect rect = ImGui::GetCurrentWindow()->TitleBarRect();
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::PushClipRect(rect.GetTL(), rect.GetBR(), 0);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

        ImGui::SetCursorPos(ImVec2(rect.GetWidth()-43,0));
        showTag(seq->view, gViews, "v", newView);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200),  ImVec2(600, 300));
        if (ImGui::BeginPopupContextItem(NULL, 0)) {
            viewTable();
            ImGui::EndPopup();
        }

        ImGui::SetCursorPos(ImVec2(rect.GetWidth()-29,0));
        showTag(seq->colormap, gColormaps, "c", newColormap);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200),  ImVec2(600, 300));
        if (ImGui::BeginPopupContextItem(NULL, 0)) {
            colormapTable();
            ImGui::EndPopup();
        }

        ImGui::SetCursorPos(ImVec2(rect.GetWidth()-15,0));
        showTag(seq->player, gPlayers, "p", newPlayer);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200),  ImVec2(600, 300));
        if (ImGui::BeginPopupContextItem(NULL, 0)) {
            playerTable();
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
        ImGui::PopClipRect();
        ImGui::SetCursorPos(pos);
    }

    if (ImGui::IsWindowFocused() && isKeyPressed(",")) {
        screenshot = true;
    }

    displaySequence(*seq);

    ImGui::End();
}

static void drawGreenRect(ImVec2 from, ImVec2 to)
{
    static ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
    static ImU32 black = ImGui::GetColorU32(ImVec4(0,0,0,1));
    ImGui::GetWindowDrawList()->AddRect(from, to, black, 0, ~0, 2.5f);
    ImGui::GetWindowDrawList()->AddRect(from, to, green);
}

static void drawGreenText(const std::string& text, ImVec2 pos)
{
    static ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
    static ImU32 black = ImGui::GetColorU32(ImVec4(0,0,0,1));
    const char* buf = text.c_str();
    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
        if (dx || dy)
            ImGui::GetWindowDrawList()->AddText(pos+ImVec2(dx, dy), black, buf);
    ImGui::GetWindowDrawList()->AddText(pos, green, buf);
}

void Window::displaySequence(Sequence& seq)
{
    View* view = seq.view;

    bool focusedit = false;
    float factor = seq.getViewRescaleFactor();
    ImRect clip = getClipRect();
    ImVec2 winSize = clip.Max - clip.Min;

    ImVec2 delta = ImGui::GetIO().MouseDelta;
    bool dragging = ImGui::IsMouseDown(0) && (delta.x || delta.y);
    if (seq.colormap && seq.view && seq.player) {
        if (gShowImage && seq.colormap->shader) {
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            displayarea.draw(seq.getCurrentImage(), clip.Min, winSize, seq.colormap, seq.view, factor);
            ImGui::PopClipRect();
        }

        std::vector<const SVG*> svgs = seq.getCurrentSVGs();
        if (!svgs.empty()) {
            ImVec2 TL = view->image2window(seq.view->svgOffset, displayarea.getCurrentSize(), winSize, factor);
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            for (int i = 0; i < svgs.size(); i++) {
                if (svgs[i] && (i >= 9 || gShowSVGs[i]))
                    svgs[i]->draw(clip.Min, TL, seq.view->zoom*factor);
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
            ImVec2 from = gSelectionFrom + off1;
            ImVec2 to = gSelectionTo + off2;
            ImVec2 fromwin = view->image2window(from, displayarea.getCurrentSize(), winSize, factor);
            ImVec2 towin = view->image2window(to, displayarea.getCurrentSize(), winSize, factor);
            fromwin += clip.Min;
            towin += clip.Min;
            drawGreenRect(fromwin, towin);

            char buf[2048];
            snprintf(buf, sizeof(buf), "%d %d", (int)from.x, (int)from.y);
            drawGreenText(buf, fromwin);
            snprintf(buf, sizeof(buf), "  %d %d (w:%d,h:%d,d:%.2f)", (int)to.x, (int)to.y,
                     (int)std::abs((to-from).x),
                     (int)std::abs((to-from).y),
                     std::sqrt(ImLengthSqr(to - from)));
            drawGreenText(buf, towin);

            if (!ImGui::IsMouseDown(1) && !screenshot) {
                ImVec2 orderedfrom(std::min(gSelectionFrom.x, gSelectionTo.x),
                                   std::min(gSelectionFrom.y, gSelectionTo.y));
                ImVec2 orderedto(std::max(gSelectionFrom.x, gSelectionTo.x),
                                 std::max(gSelectionFrom.y, gSelectionTo.y));
                auto oldpos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(towin - clip.Min + ImVec2(10, 0));
                if (ImGui::Button("fit colormap")) {
                    seq.autoScaleAndBias(orderedfrom, orderedto, 0.f);
                }
                if (ImGui::SameLine(),ImGui::Button("fit view")) {
                    seq.view->center = (orderedfrom + orderedto+ImVec2(1,1)) / (displayarea.getCurrentSize() * 2.);
                    seq.view->setOptimalZoom(contentRect.GetSize(), orderedto - orderedfrom+ImVec2(1,1), factor);
                }
                ImGui::SetCursorPos(oldpos);
            }
        }

        if (!screenshot) {
            ImVec2 from = view->image2window(gHoveredPixel, displayarea.getCurrentSize(), winSize, factor);
            ImVec2 to = view->image2window(gHoveredPixel+ImVec2(1,1), displayarea.getCurrentSize(), winSize, factor);
            from += clip.Min;
            to += clip.Min;
            if (from.x+1.f == to.x && from.y+1.f == to.y) {
                // somehow this is necessary, otherwise the square disappear :(
                to.x += 1e-3;
                to.y += 1e-3;
            }
            drawGreenRect(from, to);
        }

        if (seq.imageprovider && !seq.imageprovider->isLoaded()) {
            ImVec2 pos = ImGui::GetCursorPos();
            const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            const ImU32 bg = ImColor(100,100,100);
            ImGui::BufferingBar("##bar", seq.imageprovider->getProgressPercentage(),
                                ImVec2(ImGui::GetWindowWidth(), 6), bg, col);
            ImGui::SetCursorPos(pos);
        }

        if (seq.image && !screenshot && gShowMiniview) {
            std::shared_ptr<Image> image = seq.image;
            float r = (float) image->h / image->w;
            int w = 82;
            float alpha = gShowView > 20 ? 1. : gShowView/20.;
            ImU32 gray = ImGui::GetColorU32(ImVec4(1,1,1,0.6*alpha));
            ImU32 black = ImGui::GetColorU32(ImVec4(0,0,0,0.4*alpha));
            ImVec2 size = ImVec2(w, r*w);
            ImVec2 p1 = view->window2image(ImVec2(0, 0), displayarea.getCurrentSize(), winSize, factor);
            ImVec2 p2 = view->window2image(winSize, displayarea.getCurrentSize(), winSize, factor);
            p1 = p1 * size / displayarea.getCurrentSize();
            p2 = p2 * size / displayarea.getCurrentSize();
            int border = 5;
            ImVec2 pos(clip.Max.x - size.x - border, clip.Min.y + border);
            ImRect rout(pos, pos + size);
            ImRect rin(rout.Min+p1, rout.Min+p2);
            rin.ClipWithFull(rout);
            bool hovered = ImGui::IsMouseHoveringRect(rout.Min, rout.Max);
            if (hovered) {
                gShowView = MAX_SHOWVIEW;
            }
            if ((rin.GetWidth() < rout.GetWidth() || rin.GetHeight() < rout.GetHeight()) && gShowView) {
                ImGui::GetWindowDrawList()->AddRectFilled(rout.Min, rout.Max, black);
                ImGui::GetWindowDrawList()->AddRectFilled(rin.Min, rin.Max, gray);
                ImGui::GetWindowDrawList()->AddRect(rout.Min, rout.Max, gray, 0.f,
                                                    ImDrawCornerFlags_All, 1.f);
                if (hovered && ImGui::IsMouseDown(0)) {
                    ImVec2 p = (ImGui::GetMousePos() - rout.Min) / size
                        * displayarea.getCurrentSize() / seq.image->size;
                    seq.view->center = p;
                    dragging = false;
                }
            }
        }
    }

    static bool showthings = false;
    if (ImGui::IsWindowFocused() && isKeyPressed("F6")) {
        showthings = !showthings;
    }
    if (showthings) {
        ImGui::BeginChildFrame(ImGui::GetID(".."), ImVec2(0, size.y * 0.25));
        for (size_t i = 0; i < sequences.size(); i++) {
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

    if (!seq.valid || !seq.player)
        return;

    auto f = config::get_lua()["on_window_tick"];
    if (f) {
        f(this, ImGui::IsWindowFocused());
    }

    if (ImGui::IsWindowFocused()) {
        bool resetSat = false;

        bool zooming = isKeyDown("z");

        ImRect winclip = getClipRect();
        ImVec2 cursor = ImGui::GetMousePos() - winclip.Min;
        ImVec2 im = ImFloor(view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor));
        gHoveredPixel = im;

        if (zooming && ImGui::GetIO().MouseWheel != 0.f) {
            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);

            view->changeZoom(view->zoom * (1 + 0.1 * ImGui::GetIO().MouseWheel));

            ImVec2 pos2 = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
            view->center += (pos - pos2) / displayarea.getCurrentSize();
            gShowView = MAX_SHOWVIEW;
        }
        if (isKeyPressed("i")) {
            view->changeZoom(std::pow(2, floor(log2(view->zoom) + 1)));
            gShowView = MAX_SHOWVIEW;
        }
        if (isKeyPressed("o")) {
            view->changeZoom(std::pow(2, ceil(log2(view->zoom) - 1)));
            gShowView = MAX_SHOWVIEW;
        }

        if (!ImGui::IsMouseClicked(0) && dragging) {
            ImVec2 pos = view->window2image(ImVec2(0, 0), displayarea.getCurrentSize(), winSize, factor);
            ImVec2 pos2 = view->window2image(delta, displayarea.getCurrentSize(), winSize, factor);
            ImVec2 diff = pos - pos2;
            view->center += diff / displayarea.getCurrentSize();
            gShowView = MAX_SHOWVIEW;
        }

        if (ImGui::IsWindowHovered() || (dragging && !ImGui::IsAnyItemActive())) {
            if (ImGui::IsMouseClicked(1)) {
                gSelecting = true;

                ImRect clip = getClipRect();
                ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
                ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
                gSelectionFrom = ImFloor(pos);
                gSelectionShown = true;
            }
        }

        if (gSelecting) {
            if (ImGui::IsMouseDown(1)) {
                ImRect clip = getClipRect();
                ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
                ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
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

        if (gSelectionShown) {
            for (auto win : gWindows) {
                Sequence* s = win->getCurrentSequence();
                if (!s) continue;
                std::shared_ptr<Image> img = s->getCurrentImage();
                if (!img) continue;
                ImRect rect(gSelectionFrom, gSelectionTo);
                if (rect.Max.x < rect.Min.x)
                    std::swap(rect.Min.x, rect.Max.x);
                if (rect.Max.y < rect.Min.y)
                    std::swap(rect.Min.y, rect.Max.y);
                rect.Max.x += 1;
                rect.Max.y += 1;
                rect.ClipWithFull(ImRect(0,0,img->w,img->h));
                win->histogram->request(img, img->min, img->max,
                                        gSmoothHistogram ? Histogram::SMOOTH : Histogram::EXACT,
                                        rect);
            }
        }

        if (isKeyPressed("r")) {
            view->center = ImVec2(0.5, 0.5);
            gShowView = MAX_SHOWVIEW;
            if (isKeyDown("shift")) {
                view->resetZoom();
            } else {
                view->setOptimalZoom(contentRect.GetSize(), displayarea.getCurrentSize(), factor);
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsWindowHovered()
            && !zooming && (ImGui::GetIO().MouseWheel || ImGui::GetIO().MouseWheelH)) {
            static float delta_r = 1.f / 255.f;
            static float delta_c = 1.f / 255.f;
            std::shared_ptr<Image> img = seq.getCurrentImage();
            if (img) {
                if (isKeyDown("shift")) {
                    seq.colormap->radius = std::max(0.f, seq.colormap->radius / (1.f - 2.f * delta_r * ImGui::GetIO().MouseWheel));
                } else {
                    for (int i = 0; i < 3; i++) {
                        float newcenter = seq.colormap->center[i] + 2.f * seq.colormap->radius * delta_c * ImGui::GetIO().MouseWheel;
                        seq.colormap->center[i] = std::min(std::max(newcenter, img->min), img->max);
                    }
                }
                seq.colormap->radius = std::max(0.f, seq.colormap->radius / (1.f - 2.f * delta_r * ImGui::GetIO().MouseWheelH));
                resetSat = true;
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && (delta.x || delta.y) && isKeyDown("shift")) {
            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
            std::shared_ptr<Image> img = seq.getCurrentImage();
            if (img && pos.x >= 0 && pos.y >= 0 && pos.x < img->w && pos.y < img->h) {
                std::array<float,3> v{};
                int n = std::min(img->c, (size_t)3);
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
                resetSat = true;
            }
        }

        if (seq.player) {
            // TODO: this delays the actual change of frame when we press left/right
            seq.player->checkShortcuts();

            if (isKeyPressed("!")) {
                seq.removeCurrentFrame();
            }
        }

        if (isKeyPressed("a")) {
            resetSat = true;
            if (isKeyDown("shift")) {
                seq.snapScaleAndBias();
            } else {
                ImVec2 p1(0, 0);
                ImVec2 p2(0, 0);
                float sat = 0.f;
                if (isKeyDown("control")) {
                    p1 = view->window2image(ImVec2(0, 0), displayarea.getCurrentSize(), winSize, factor);
                    p2 = view->window2image(winSize, displayarea.getCurrentSize(), winSize, factor);
                }
                if (isKeyDown("alt")) {
                    auto& L = config::get_lua();
                    std::vector<float> s = L["SATURATIONS"];
                    sat = s[seq.colormap->currentSat];
                    seq.colormap->currentSat = (seq.colormap->currentSat + 1) % s.size();
                    resetSat = false;
                }
                seq.autoScaleAndBias(p1, p2, sat);
            }
        }
        if (resetSat) {
            seq.colormap->currentSat = 0;
        }

        if (isKeyPressed("s") && !isKeyDown("control")) {
            if (isKeyDown("shift")) {
                seq.colormap->previousShader();
            } else {
                seq.colormap->nextShader();
            }
        }

        if (isKeyPressed("e")) {
            if (!seq.editGUI->isEditing()) {
                EditType type = PLAMBDA;
                if (isKeyDown("shift")) {
#ifdef USE_GMIC
                    type = EditType::GMIC;
#else
                    std::cerr << "GMIC isn't enabled, check your compilation." << std::endl;
#endif
                } else if (isKeyDown("control")) {
#ifdef USE_OCTAVE
                    type = EditType::OCTAVE;
#else
                    std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
                }
                seq.setEdit(std::to_string(seq.getId()), type);
            }
            focusedit = true;
        }
    }

    if (!screenshot) {
        seq.editGUI->display(seq, focusedit);
    }

    if (!seq.error.empty()) {
        ImGui::TextColored(ImColor(255, 0, 0), "error: %s", seq.error.c_str());
    }

    if (gShowHud && seq.image && !screenshot) {
        displayInfo(seq);
    }
}

void Window::displayInfo(Sequence& seq)
{
    ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetCursorPos());
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

    float v[4] = {0};
    bool highlights = false;
    if (!seq.valid)
        goto end;

    ImGui::Separator();

    {
        ImVec2 im = gHoveredPixel;
        ImGui::Text("Pixel: x:%d, y:%d", (int)im.x, (int)im.y);

        std::shared_ptr<Image> img = seq.getCurrentImage();
        if (img && im.x >= 0 && im.y >= 0 && im.x < img->w && im.y < img->h) {
            highlights = true;
            img->getPixelValueAt(im.x, im.y, v, 4);
            if (img->c == 1) {
                ImGui::Text("Gray: %g", v[0]);
            } else if (img->c == 2) {
                ImGui::Text("Flow: %g, %g", v[0], v[1]);
            } else if (img->c == 3) {
                ImGui::Text("RGB: %g, %g, %g", v[0], v[1], v[2]);
            } else if (img->c == 4) {
                ImGui::Text("RGBA: %g, %g, %g, %g", v[0], v[1], v[2], v[3]);
            }
        } //else { ImGui::Text(""); }
    }

    if (gShowHistogram) {
        std::array<float,3> cmin, cmax;
        seq.colormap->getRange(cmin, cmax);

        std::shared_ptr<Image> img = seq.getCurrentImage();
        if (img) {
            std::shared_ptr<Histogram> imghist = img->histogram;
            imghist->draw(cmin, cmax, highlights ? v : 0);
            if (gSelectionShown) {
                histogram->draw(cmin, cmax, highlights ? v : 0);
            }
        }
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
    size_t x = contentRect.Min.x;
    size_t y = winSize.y - contentRect.Max.y;
    size_t w = contentRect.Max.x - contentRect.Min.x;
    size_t h = contentRect.Max.y - contentRect.Min.y;
    x *= ImGui::GetIO().DisplayFramebufferScale.x;
    w *= ImGui::GetIO().DisplayFramebufferScale.x;
    y *= ImGui::GetIO().DisplayFramebufferScale.y;
    h *= ImGui::GetIO().DisplayFramebufferScale.y;
    size_t size = 3 * w * h;

    float* data = new float[size];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(x, y, w, h, GL_RGB, GL_FLOAT, data);
    for (size_t i = 0; i < size; i++)
        data[i] *= 255.f;
    for (size_t y = 0; y < h/2; y++) {
        for (size_t i = 0; i < 3*w; i++) {
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
    const int tagssize = 50;
    int w = size.x - tagssize;
    int n = w / 5;
    std::string title = seq->getTitle(n);
    ImVec2 size = ImGui::CalcTextSize(title.c_str());
    while (n > 0 && size.x > w) {
        n--;
        title = seq->getTitle(n);
        size = ImGui::CalcTextSize(title.c_str());
    }
    return title;
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

