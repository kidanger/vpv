#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Window/Event.hpp>
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "globals.hpp"
#include "Window.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "Player.hpp"

#include "shaders.cpp"

static ImRect getRenderingRect(ImVec2 texSize, ImRect* windowRect=0);

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

    //FIXME
    Sequence* sq;
    for (auto s : sequences)
        if (s->visible) sq = s;
    Sequence& seq = *sq;

    Texture& texture = seq.texture;
    View* view = seq.view;

    if (!seq.valid || !seq.player) {
        return;
    }

    if (forceGeometry) {
        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSize(size);
        forceGeometry = false;
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", mode->getTitle(*this).c_str(), ID.c_str());
    if (!ImGui::Begin(buf, &opened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return;
    }

    int index = std::find(gWindows.begin(), gWindows.end(), this) - gWindows.begin();
    if (index <= 9 && ImGui::IsKeyPressed(sf::Keyboard::Num1 + index)
        && !ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
        ImGui::SetWindowFocus();
    }

    if (sequences.size()) {
        mode->display(*this);

        if (ImGui::IsWindowFocused()) {
            bool zooming = ImGui::IsKeyDown(sf::Keyboard::Z);
            if (!ImGui::IsMouseDown(2) && zooming && ImGui::GetIO().MouseWheel != 0.f) {
                view->zoom *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
                printf("Zoom: %g\n", view->zoom);
            }

            ImVec2 drag = ImGui::GetMouseDragDelta(0, 0.f);
            if (drag.x || drag.y) {
                ImGui::ResetMouseDragDelta(0);
                view->center -= drag / view->zoom / 1.18;
            }

            if (ImGui::IsMouseDown(2)) {
                if (zooming && ImGui::GetIO().MouseWheel != 0.f) {
                    view->smallzoomfactor *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
                }

                ImVec2 u, v;
                view->compute(texture.getSize(), u, v);

                ImRect renderingRect = getRenderingRect(texture.size);
                ImVec2 r = (ImGui::GetMousePos() - renderingRect.Min) / renderingRect.GetSize();
                ImGui::BeginTooltip();
                {
                    ImVec2 center = u * (1.f - r) + v * r;
                    float halfoffset = 1 / (2 * view->zoom*view->smallzoomfactor);
                    ImVec2 uu = center - halfoffset;
                    ImVec2 vv = center + halfoffset;

                    float texw = (float) texture.getSize().x;
                    float texh = (float) texture.getSize().y;
                    ImGui::GetWindowDrawList()->CmdBuffer.back().shader = &gShaders[seq.shader];
                    ImGui::GetWindowDrawList()->CmdBuffer.back().scale = seq.scale;
                    ImGui::GetWindowDrawList()->CmdBuffer.back().bias = seq.bias;
                    ImGui::Image((void*)(size_t)texture.id, ImVec2(128, 128*texh/texw), uu, vv);

                    int x = (uu.x+vv.x)/2*texw;
                    int y = (uu.y+vv.y)/2*texh;
                    ImGui::Text("(%d, %d)", x, y);

                    Image* img = seq.getCurrentImage();
                    if (img) {
                        float v[4] = {0};
                        seq.getPixelValueAt(x, y, v, 4);
                        if (img->format == Image::R) {
                            ImGui::Text("gray (%g)", v[0]);
                        } else if (img->format == Image::RG) {
                            ImGui::Text("flow (%g, %g)", v[0], v[1]);
                        } else if (img->format == Image::RGB) {
                            ImGui::Text("rgb (%g, %g, %g)", v[0], v[1], v[2]);
                        } else if (img->format == Image::RGBA) {
                            ImGui::Text("rgba (%g, %g, %g, %g)", v[0], v[1], v[2], v[3]);
                        }
                    }
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
            if (!zooming) {
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    seq.bias += 0.1 * ImGui::GetIO().MouseWheel;
                } else {
                    seq.scale += 0.001 * ImGui::GetIO().MouseWheel;
                }
            }
            if (seq.player) {
                seq.player->checkShortcuts();
            }
            if (ImGui::IsKeyPressed(sf::Keyboard::A)) {
                seq.autoScaleAndBias();
            }
            if (ImGui::IsKeyPressed(sf::Keyboard::S)) {
                extern std::map<std::string, sf::Shader> gShaders;
                bool next = false;
                bool done = false;
                for (auto& it : gShaders) {
                    if (next) {
                        seq.shader = it.first;
                        done = true;
                        break;
                    }
                    if (it.first == seq.shader) {
                        next = true;
                    }
                }
                if (!done) {
                    seq.shader = gShaders.begin()->first;
                }
                printf("shader: %s\n", seq.shader.c_str());
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


const std::string& FlipWindowMode::getTitle(const Window& window)
{
    const Sequence* seq = window.sequences[index];
    return seq->filenames[seq->player->frame - 1];
}

void FlipWindowMode::display(Window& window)
{
    Sequence& seq = *window.sequences[index];
    Texture& texture = seq.texture;
    View* view = seq.view;

    ImVec2 u, v;
    view->compute(texture.getSize(), u, v);

    ImRect clip;
    ImRect position = getRenderingRect(texture.size, &clip);
    ImGui::PushClipRect(clip.Min, clip.Max, true);
    ImGui::GetWindowDrawList()->CmdBuffer.back().shader = &gShaders[seq.shader];
    ImGui::GetWindowDrawList()->CmdBuffer.back().scale = seq.scale;
    ImGui::GetWindowDrawList()->CmdBuffer.back().bias = seq.bias;
    ImGui::GetWindowDrawList()->AddImage((void*)(size_t)texture.id, position.Min, position.Max, u, v);
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

ImRect getRenderingRect(ImVec2 texSize, ImRect* windowRect)
{
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 pos2 = pos + ImGui::GetContentRegionAvail();
    if (windowRect) {
        *windowRect = ImRect(pos, pos2);
    }

    ImVec2 diff = pos2 - pos;
    float aspect = (float) texSize.x / texSize.y;
    float nw = fmax(diff.x, diff.y * aspect);
    float nh = fmax(diff.y, diff.x / aspect);
    ImVec2 offset = ImVec2(nw - diff.x, nh - diff.y);

    pos -= offset / 2;
    pos2 += offset / 2;
    return ImRect(pos, pos2);
}
