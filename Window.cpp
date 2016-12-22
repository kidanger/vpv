#include <fstream>
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

#include "shaders.cpp"

static ImRect getRenderingRect(ImVec2 texSize, ImRect* windowRect=0);
static ImVec2 fromWindowToImage(const ImVec2& win, const ImVec2& texSize, const View& view, float additionalZoom=1.f);

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

    mode = new FlipWindowMode;
    opened = true;
}

void Window::display()
{
    screenshot = false;

    if (!opened) {
        return;
    }

    //FIXME
    Sequence* sq = nullptr;
    for (auto s : sequences)
        if (s->visible) sq = s;
    if (!sq)
        return;
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
    if (!ImGui::Begin(buf, &opened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse)) {
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
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            bool showingTooltip = ImGui::IsMouseDown(2) || ImGui::IsKeyDown(sf::Keyboard::T);
            bool zooming = ImGui::IsKeyDown(sf::Keyboard::Z);

            if (!showingTooltip) {
                if (zooming && ImGui::GetIO().MouseWheel != 0.f) {
                    view->changeZoom(view->zoom * (1 + 0.1 * ImGui::GetIO().MouseWheel));
                }
                if (ImGui::IsKeyPressed(sf::Keyboard::I)) {
                    view->changeZoom(std::pow(2, floor(log2(view->zoom) + 1)));
                }
                if (ImGui::IsKeyPressed(sf::Keyboard::O)) {
                    view->changeZoom(std::pow(2, ceil(log2(view->zoom) - 1)));
                }
            }

            if (ImGui::IsMouseDown(0) && (delta.x || delta.y)) {
                ImVec2 pos = fromWindowToImage(ImGui::GetMousePos(), texture.getSize(), *view);
                ImVec2 pos2 = fromWindowToImage(ImGui::GetMousePos() + delta, texture.getSize(), *view);
                view->center += pos - pos2;
            }

            if (showingTooltip) {
                if (zooming && ImGui::GetIO().MouseWheel != 0.f) {
                    view->smallzoomfactor *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
                }
                if (ImGui::IsKeyPressed(sf::Keyboard::I)) {
                    view->smallzoomfactor = std::pow(2, floor(log2(view->smallzoomfactor) + 1));
                }
                if (ImGui::IsKeyPressed(sf::Keyboard::O)) {
                    view->smallzoomfactor = std::pow(2, ceil(log2(view->smallzoomfactor) - 1));
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
                    ImGui::GetWindowDrawList()->CmdBuffer.back().shader = &gShaders[seq.colormap->getShaderName()];
                    ImGui::GetWindowDrawList()->CmdBuffer.back().scale = seq.colormap->scale;
                    ImGui::GetWindowDrawList()->CmdBuffer.back().bias = seq.colormap->bias;
                    ImGui::Image((void*)(size_t)texture.id, ImVec2(128, 128*texh/texw), uu, vv);

                    int x = std::floor((uu.x+vv.x)/2*texw);
                    int y = std::floor((uu.y+vv.y)/2*texh);
                    ImGui::Text("(%d, %d)", x, y);

                    Image* img = seq.getCurrentImage();
                    if (img && x >= 0 && y >= 0 && x < img->w && y < img->h) {
                        float v[4] = {0};
                        img->getPixelValueAt(x, y, v, 4);
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

            if (ImGui::IsKeyPressed(sf::Keyboard::R)) {
                view->center = texture.getSize() / 2;
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    view->resetZoom();
                } else {
                    view->setOptimalZoom(contentRect.GetSize(), texture.getSize());
                }
            }
            if (!zooming && ImGui::GetIO().MouseWheel) {
                Image* img = seq.getCurrentImage();
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    if (std::abs(seq.colormap->scale) < 1e-6)
                        seq.colormap->scale = 1e-6 * ImGui::GetIO().MouseWheel;
                    else
                        seq.colormap->scale += std::abs(seq.colormap->scale) * 0.1 * ImGui::GetIO().MouseWheel;
                } else {
                    printf("%g %g\n", img->max, img->min);
                    seq.colormap->bias += seq.colormap->scale * (img->max - img->min) * 0.05 * ImGui::GetIO().MouseWheel;
                }
                printf("scale: %g, bias: %g\n", seq.colormap->scale, seq.colormap->bias);
            }

            if (ImGui::IsKeyDown(sf::Keyboard::LShift) && (delta.x || delta.y)) {
                ImVec2 pos = fromWindowToImage(ImGui::GetMousePos(), texture.getSize(), *view);
                Image* img = seq.getCurrentImage();
                if (img && pos.x >= 0 && pos.y >= 0 && pos.x < img->w && pos.y < img->h) {
                    int nb = img->format;
                    float v[nb];
                    memset(v, 0, nb*sizeof(float));
                    img->getPixelValueAt(pos.x, pos.y, v, nb);
                    float mean = (v[0]*(nb>=1) + v[1]*(nb>=2) + v[2]*(nb>=3) + v[3]*(nb>=4)) / nb;
                    if (!isnan(mean) && !isinf(mean)) {
                        seq.colormap->bias = 0.5f - mean * seq.colormap->scale;
                        printf("scale: %g, bias: %g\n", seq.colormap->scale, seq.colormap->bias);
                    }
                }
            }

            if (seq.player) {
                seq.player->checkShortcuts();
            }
            if (ImGui::IsKeyPressed(sf::Keyboard::A)) {
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    Image* img = seq.getCurrentImage();
                    if (img && img->max > 1.f)
                        seq.colormap->scale = 1/256.f;
                    else
                        seq.colormap->scale = 1.f;
                    seq.colormap->bias = 0;
                } else {
                    seq.autoScaleAndBias();
                }
                printf("scale: %g, bias: %g\n", seq.colormap->scale, seq.colormap->bias);
            }
            if (ImGui::IsKeyPressed(sf::Keyboard::S)) {
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    seq.colormap->tonemap = (Colormap::Tonemap) ((Colormap::NUM_TONEMAPS + seq.colormap->tonemap - 1) % Colormap::NUM_TONEMAPS);
                } else {
                    seq.colormap->tonemap = (Colormap::Tonemap) ((seq.colormap->tonemap + 1) % Colormap::NUM_TONEMAPS);
                }
                printf("shader: %s\n", seq.colormap->getShaderName().c_str());
            }

            if (ImGui::IsKeyPressed(sf::Keyboard::Comma)) {
                screenshot = true;
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

    const char* filename_fmt = getenv("SCREENSHOT");
    if (!filename_fmt) filename_fmt = "screenshot_%d.png";
    int i = 1;
    while (true) {
        char filename[512];
        snprintf(filename, sizeof(filename), filename_fmt, i);
        if (!file_exists(filename)) {
            iio_save_image_float_vec(filename, data, w, h, 3);
            printf("Screenshot saved to '%s'.\n", filename);
            break;
        }
        i++;
    }
    delete[] data;
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
    ImGui::GetWindowDrawList()->CmdBuffer.back().shader = &gShaders[seq.colormap->getShaderName()];
    ImGui::GetWindowDrawList()->CmdBuffer.back().scale = seq.colormap->scale;
    ImGui::GetWindowDrawList()->CmdBuffer.back().bias = seq.colormap->bias;
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

    window.contentRect = clip;
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
    float nw = std::max(diff.x, diff.y * aspect);
    float nh = std::max(diff.y, diff.x / aspect);
    ImVec2 offset = ImVec2(nw - diff.x, nh - diff.y);

    pos -= offset / 2;
    pos2 += offset / 2;
    return ImRect(pos, pos2);
}

ImVec2 fromWindowToImage(const ImVec2& win, const ImVec2& texSize, const View& view, float additionalZoom) {
    ImVec2 u, v;
    view.compute(texSize, u, v);
    ImRect renderingRect = getRenderingRect(texSize);
    ImVec2 r = (win - renderingRect.Min) / renderingRect.GetSize();
    ImVec2 center = u * (1.f - r) + v * r;
    float halfoffset = 1 / (2 * view.zoom*additionalZoom);
    ImVec2 uu = center - halfoffset;
    ImVec2 vv = center + halfoffset;
    return (uu + vv) / 2 * texSize;
}
