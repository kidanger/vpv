#include <cmath>
#include <string>
#include <glob.h>
#include <iostream>
#include <cfloat>
#include <algorithm>
#include <map>
#include <thread>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui-SFML.h"

#include "Sequence.hpp"
#include "Window.hpp"
#include "View.hpp"
#include "Player.hpp"
#include "Colormap.hpp"
#include "Image.hpp"
#include "globals.hpp"
#include "shaders.hpp"

sf::RenderWindow* SFMLWindow;

std::vector<Sequence*> gSequences;
std::vector<View*> gViews;
std::vector<Player*> gPlayers;
std::vector<Window*> gWindows;
std::vector<Colormap*> gColormaps;

void menu();
void theme();

void frameloader()
{
    while (SFMLWindow->isOpen()) {
        for (int j = 1; j < 100; j+=10) {
            for (int i = 0; i < j; i++) {
                for (auto s : gSequences) {
                    if (s->valid && s->player) {
                        int frame = s->player->frame + i;
                        if (frame >= s->player->minFrame && frame <= s->player->maxFrame) {
                            Image::load(s->filenames[frame - 1]);
                        }
                    }
                }
            }
            sf::sleep(sf::milliseconds(5));
        }
    }
}

void parseArgs(int argc, char** argv)
{
    View* view = new View;
    gViews.push_back(view);

    Player* player = new Player;
    gPlayers.push_back(player);

    Window* window = new Window;
    gWindows.push_back(window);

    Colormap* colormap = new Colormap;
    gColormaps.push_back(colormap);

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "nv") {
            view = new View;
            gViews.push_back(view);
        } else if (arg == "np") {
            player = new Player;
            gPlayers.push_back(player);
        } else if (arg == "nw") {
            window = new Window;
            gWindows.push_back(window);
        } else if (arg == "nc") {
            colormap = new Colormap;
            gColormaps.push_back(colormap);
        } else {
            Sequence* seq = new Sequence;
            gSequences.push_back(seq);

            strncpy(&seq->glob[0], arg.c_str(), seq->glob.capacity());
            seq->loadFilenames();

            seq->view = view;
            seq->player = player;
            seq->player->configureWithSequence(*seq);
            seq->colormap = colormap;
            window->sequences.push_back(seq);
            window->mode->onAddSequence(*window, seq);
        }
    }
}

enum Layout {
    HORIZONTAL, VERTICAL, GRID, FREE, FULLSCREEN,
    NUM_LAYOUTS,
};
Layout currentLayout = HORIZONTAL;
std::map<Layout, std::string> layoutNames = {
    {HORIZONTAL, "horizontal"},
    {VERTICAL, "vertical"},
    {GRID, "grid"},
    {FREE, "free"},
    {FULLSCREEN, "fullscreen"},
};
void relayout();

int main(int argc, char** argv)
{
    SFMLWindow = new sf::RenderWindow(sf::VideoMode(640, 480), "Video Viewer");
    SFMLWindow->setVerticalSyncEnabled(true);
    loadShaders();

    ImGui::SFML::Init(*SFMLWindow);
    theme();

    parseArgs(argc, argv);

    for (auto seq : gSequences) {
        seq->loadTextureIfNeeded();
        if (seq->view->center.x == 0 && seq->view->center.y == 0) {
            seq->view->center = seq->texture.getSize() / 2;
        }
        seq->autoScaleAndBias();
    }

    relayout();

    std::thread th(frameloader);

    sf::Clock deltaClock;
    while (SFMLWindow->isOpen()) {
        sf::Event event;
        while (SFMLWindow->pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                SFMLWindow->close();
            } else if (event.type == sf::Event::Resized) {
                if (currentLayout != FREE) {
                    relayout();
                }
            }
        }

        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(dt);

        for (auto p : gPlayers) {
            p->update();
        }
        for (auto w : gWindows) {
            w->display();
        }
        menu();

        for (auto seq : gSequences) {
            seq->loadTextureIfNeeded();
        }

        static bool showfps = 0;
        if (showfps || ImGui::IsKeyPressed(sf::Keyboard::F12))
        {
            showfps = 1;
            if (ImGui::Begin("FPS", &showfps)) {
                const int num = 100;
                static float fps[num] = {0};
                memcpy(fps, fps+1, sizeof(float)*(num - 1));
                fps[num - 1] = dt.asMilliseconds();
                ImGui::PlotLines("FPS", fps, num, 0, 0, 10, 40);
                ImGui::End();
            }
        }

        if (ImGui::IsKeyDown(sf::Keyboard::LControl) && ImGui::IsKeyPressed(sf::Keyboard::L)) {
            currentLayout = (Layout) ((currentLayout + 1) % NUM_LAYOUTS);
            relayout();
            printf("current layout: %s\n", layoutNames[currentLayout].c_str());
        }

        SFMLWindow->clear();
        ImGui::Render();
        SFMLWindow->display();
    }

    th.join();
    ImGui::SFML::Shutdown();
    delete SFMLWindow;
}

namespace ImGui {
    void ShowTestWindow(bool* p_open);
}

static bool debug = false;

void menu()
{
    if (debug) ImGui::ShowTestWindow(&debug);

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Players")) {
            for (auto p : gPlayers) {
                if (ImGui::BeginMenu(p->ID.c_str())) {
                    ImGui::MenuItem("Opened", nullptr, &p->opened);
                    p->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New player")) {
                Player* p = new Player;
                p->opened = true;
                gPlayers.push_back(p);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sequences")) {
            for (auto s : gSequences) {
                if (ImGui::BeginMenu(s->ID.c_str())) {
                    ImGui::Text(s->glob.c_str());

                    if (!s->valid) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "INVALID");
                    }

                    ImGui::Spacing();

                    if (ImGui::BeginMenu("Attached player")) {
                        for (auto p : gPlayers) {
                            bool attached = p == s->player;
                            if (ImGui::MenuItem(p->ID.c_str(), 0, attached)) {
                                s->player = p;
                                s->player->configureWithSequence(*s);
                            }
                            if (ImGui::BeginPopupContextItem(p->ID.c_str())) {
                                p->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Attached view")) {
                        for (auto v : gViews) {
                            bool attached = v == s->view;
                            if (ImGui::MenuItem(v->ID.c_str(), 0, attached)) {
                                s->view = v;
                            }
                            if (ImGui::BeginPopupContextItem(v->ID.c_str())) {
                                v->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                        ImGui::EndMenu();
                    }

                    ImGui::Spacing();

                    ImGui::PushItemWidth(400);
                    if (ImGui::InputText("File glob", &s->glob_[0], s->glob_.capacity(),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        strcpy(&s->glob[0], &s->glob_[0]);
                        s->loadFilenames();
                    }
                    if (ImGui::BeginPopupContextItem(s->ID.c_str())) {
                        if (ImGui::Selectable("Reset")) {
                            strcpy(&s->glob_[0], &s->glob[0]);
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::BeginChild("scrolling", ImVec2(0, ImGui::GetItemsLineHeightWithSpacing()*5 + 20),
                                      false, ImGuiWindowFlags_HorizontalScrollbar);
                    glob_t res;
                    ::glob(s->glob_.c_str(), GLOB_TILDE, NULL, &res);
                    for(unsigned int j = 0; j < res.gl_pathc; j++) {
                        if (ImGui::Selectable(res.gl_pathv[j], false)) {
                            strcpy(&s->glob_[0], res.gl_pathv[j]);
                        }
                    }
                    globfree(&res);
                    ImGui::EndChild();

                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("Load new sequence")) {
                Sequence* s = new Sequence;
                s->view = gViews[0];
                gSequences.push_back(s);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Views")) {
            for (auto v : gViews) {
                if (ImGui::BeginMenu(v->ID.c_str())) {
                    v->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New view")) {
                View* v = new View;
                gViews.push_back(v);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            for (auto w : gWindows) {
                if (ImGui::BeginMenu(w->ID.c_str())) {
                    w->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New window")) {
                Window* w = new Window;
                gWindows.push_back(w);
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Debug", nullptr, &debug)) debug = true;

        ImGui::EndMainMenuBar();
    }
}

void steplayout(ImVec2 start, ImVec2 end, ImVec2 step, const std::vector<Window*>& windows)
{
    ImVec2 individualSize;
    if (step.x) {
        individualSize = ImVec2(step.x, end.y - start.y);
    } else if (step.y) {
        individualSize = ImVec2(end.x - start.x, step.y);
    } else {
        individualSize = end - start;
    }
    for (auto w : windows) {
        w->position = start;
        w->size = individualSize;
        w->forceGeometry = true;
        start += step;
    }
}

void relayout()
{
    ImVec2 menuPos = ImVec2(0, 19);
    ImVec2 size = SFMLWindow->getSize() - menuPos;

    int num = gWindows.size();
    switch (currentLayout) {
        case HORIZONTAL:
            steplayout(menuPos, menuPos + size, ImVec2(size.x / num, 0), gWindows);
            break;

        case VERTICAL:
            steplayout(menuPos, menuPos + size, ImVec2(0, size.y / num), gWindows);
            break;

        case GRID:
            {
                int n = round(sqrtf(num));
                int index = 0;

                ImVec2 _start = menuPos;
                ImVec2 _size = size / ImVec2(1, n);
                ImVec2 step = ImVec2(0, _size.y);

                for (int i = 0; i < n; i++) {
                    int endindex = index + num / n;
                    if (i == n-1)
                        endindex = num;

                    std::vector<Window*> wins(gWindows.begin() + index, gWindows.begin() + endindex);
                    ImVec2 _step = ImVec2(_size.x / wins.size(), 0);
                    steplayout(_start, _start + _size, _step, wins);
                    _start += step;

                    index = endindex;
                }
            }
            break;

        case FULLSCREEN:
            steplayout(menuPos, menuPos + size, ImVec2(), gWindows);

        case FREE:
        default:
            break;
    }
}

void theme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.WindowPadding = ImVec2(1, 1);
    style.WindowRounding = 0.f;
    style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
    style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.56f, 0.73f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.86f, 0.86f, 0.86f, 0.99f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Column]                = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.59f, 0.59f, 0.59f, 0.50f);
    style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}

