#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "globals.hpp"
#include "Window.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "Player.hpp"

static ImRect getRenderingRect(const sf::Texture& texture, ImRect* windowRect=0);

Window::Window()
{
    static int id = 0;
    id++;
    ID = "Window " + std::to_string(id);

        mode = new FlipWindowMode;
        opened = true;
}

void Window::display()
{
    if (!opened) {
        return;
    }

    Sequence& seq = *sequences[0];
    sf::Texture& texture = seq.texture;
    View* view = seq.view;

    if (!seq.valid || !seq.player) {
        return;
    }

    if (gWindows.size() > 1) {
        ImGui::SetNextWindowSize(texture.getSize(), ImGuiSetCond_FirstUseEver);
    } else {
        fullscreen(true, ImGuiSetCond_Once);
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", mode->getTitle(*this).c_str(), ID.c_str());
    if (!ImGui::Begin(buf, &opened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(sf::Keyboard::F2)) {
        fullscreen(false, ImGuiSetCond_Always);
    }

    int index = std::find(gWindows.begin(), gWindows.end(), this) - gWindows.begin();
    if (index <= 9 && ImGui::IsKeyPressed(sf::Keyboard::Num1 + index)
        && !ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
        ImGui::SetWindowFocus();
    }

    if (sequences.size()) {
        mode->display(*this);

        if (ImGui::IsWindowFocused()) {
            if (!ImGui::IsMouseDown(2) && ImGui::GetIO().MouseWheel != 0.f) {
                view->zoom *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
                printf("Zoom: %g\n", view->zoom);
            }

            ImVec2 drag = ImGui::GetMouseDragDelta(1);
            if (drag.x || drag.y) {
                ImGui::ResetMouseDragDelta(1);
                view->center -= drag / view->zoom;
            }

            if (ImGui::IsMouseDown(2)) {
                if (ImGui::GetIO().MouseWheel != 0.f) {
                    view->smallzoomfactor *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
                }

                ImVec2 u, v;
                view->compute(texture.getSize(), u, v);

                ImRect renderingRect = getRenderingRect(texture);
                ImVec2 r = (ImGui::GetMousePos() - renderingRect.Min) / renderingRect.GetSize();
                ImGui::BeginTooltip();
                {
                    ImVec2 center = u * (1.f - r) + v * r;
                    float halfoffset = 1 / (2 * view->zoom*view->smallzoomfactor);
                    ImVec2 uu = center - halfoffset;
                    ImVec2 vv = center + halfoffset;

                    // TODO: flip window mode
                    float texw = (float) texture.getSize().x;
                    float texh = (float) texture.getSize().y;
                    ImGui::Image(&texture, ImVec2(128, 128*texh/texw), uu, vv);
                    ImGui::Text("around (%.0f, %.0f)", (uu.x+vv.x)/2*texw, (uu.y+vv.y)/2*texh);
                }
                ImGui::EndTooltip();
            }

            if (ImGui::IsKeyPressed(sf::Keyboard::I)) {
                view->zoom = pow(2, floor(log2(view->zoom) + 1));
                printf("Zoom: %g\n", view->zoom);
            }
            if (ImGui::IsKeyPressed(sf::Keyboard::O)) {
                view->zoom = pow(2, ceil(log2(view->zoom) - 1));
                printf("Zoom: %g\n", view->zoom);
            }
            if (ImGui::IsKeyPressed(sf::Keyboard::R)) {
                view->zoom = 1.f;
                view->center = texture.getSize() / 2;
            }
            if (seq.player) {
                seq.player->checkShortcuts();
            }
        }
    }

    ImGui::End();
}

void Window::displaySettings()
{
    ImGui::Text("Sequences");
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

    // UGLY
    std::vector<std::string> modes = {"Flip"};
    ImGui::Text("Mode");
    for (auto& m : modes) {
        if (ImGui::RadioButton(m.c_str(), mode->type == m)) {
            if (m == "Flip") {
                setMode(new FlipWindowMode);
            }
        }
        if (mode->type == m && ImGui::BeginPopupContextItem("context")) {
            mode->displaySettings(*this);
            ImGui::EndPopup();
        }
    }
}

void Window::setMode(WindowMode* mode)
{
    if (this->mode)
        delete this->mode;
    this->mode = mode;
}

void Window::fullscreen(bool next, ImGuiSetCond cond)
{
    ImVec2 pos = ImVec2(0, 19);
    ImVec2 size = SFMLWindow->getSize() - pos;
    if (next) {
        ImGui::SetNextWindowPos(pos, cond);
        ImGui::SetNextWindowSize(size, cond);
    } else {
        ImGui::SetWindowPos(pos, cond);
        ImGui::SetWindowSize(size, cond);
    }
}


const std::string& FlipWindowMode::getTitle(const Window& window)
{
    const Sequence* seq = window.sequences[index];
    return seq->filenames[seq->player->frame - 1];
}

void FlipWindowMode::display(Window& window)
{
    Sequence& seq = *window.sequences[index];
    sf::Texture& texture = seq.texture;
    View* view = seq.view;

    ImVec2 u, v;
    view->compute(texture.getSize(), u, v);

    ImRect clip;
    ImRect position = getRenderingRect(texture, &clip);
    ImGui::PushClipRect(clip.Min, clip.Max, true);
    ImGui::GetWindowDrawList()->AddImage(&texture, position.Min, position.Max, u, v);
    ImGui::PopClipRect();

    if (ImGui::IsWindowFocused()) {
        if (ImGui::IsKeyPressed(sf::Keyboard::Space)) {
            index = (index + 1) % window.sequences.size();
        }
        if (ImGui::IsKeyPressed(sf::Keyboard::BackSpace)) {
            index = (window.sequences.size() + index - 1) % window.sequences.size();
        }
    }
    for (auto s : window.sequences) {
        s->visible = s == window.sequences[index];
    }
}

void FlipWindowMode::displaySettings(Window& window)
{
    ImGui::SliderInt("Index", &index, 0, window.sequences.size()-1);
}

void FlipWindowMode::onAddSequence(Window& window, Sequence* seq)
{
    seq->visible = window.sequences[index] == seq;
}

ImRect getRenderingRect(const sf::Texture& texture, ImRect* windowRect)
{
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 pos2 = pos + ImGui::GetContentRegionAvail();
    if (windowRect) {
        *windowRect = ImRect(pos, pos2);
    }

    ImVec2 diff = pos2 - pos;
    float aspect = (float) texture.getSize().x / texture.getSize().y;
    float nw = fmax(diff.x, diff.y * aspect);
    float nh = fmax(diff.y, diff.x / aspect);
    ImVec2 offset = ImVec2(nw - diff.x, nh - diff.y);

    pos -= offset / 2;
    pos2 += offset / 2;
    return ImRect(pos, pos2);
}
