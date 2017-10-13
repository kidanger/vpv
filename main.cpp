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
#include "Shader.hpp"
#include "Image.hpp"
#include "globals.hpp"
#include "shaders.hpp"
#include "layout.hpp"
#include "watcher.hpp"

sf::RenderWindow* SFMLWindow;

std::vector<Sequence*> gSequences;
std::vector<View*> gViews;
std::vector<Player*> gPlayers;
std::vector<Window*> gWindows;
std::vector<Colormap*> gColormaps;
std::vector<Shader*> gShaders;
bool useCache = true;
bool gSelecting;
ImVec2 gSelectionFrom;
ImVec2 gSelectionTo;
bool gSelectionShown;
ImVec2 gHoveredPixel;
bool gShowHud = true;

void menu();
void theme();

void frameloader()
{
    while (SFMLWindow->isOpen()) {
        for (int j = 1; j < 100; j+=10) {
            for (int i = 0; i < j; i++) {
                for (auto s : gSequences) {
                    if (!useCache)
                        goto sleep;
                    if (s->valid && s->player) {
                        int frame = s->player->frame + i;
                        if (frame >= s->player->minFrame && frame <= s->player->maxFrame) {
                            Image::load(s->filenames[frame - 1]);
                        }
                        if (!SFMLWindow->isOpen()) {
                            goto end;
                        }
                    }
                }
            }
sleep:
            sf::sleep(sf::milliseconds(5));
        }
    }
end: ;
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

    bool autoview = false;
    bool autoplayer = false;
    bool autowindow = false;
    bool autocolormap = false;
    bool has_one_sequence = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // (e:|E:|o:).*
        bool isedit = (arg.size() >= 2 && (arg[0] == 'e' || arg[0] == 'E' || arg[0] == 'o') && arg[1] == ':');
        // (n|a)(v|p|w|c)
        bool isnewthing = arg.size() == 2 && (arg[0] == 'n' || arg[0] == 'a')
                        && (arg[1] == 'v' || arg[1] == 'p' || arg[1] == 'w' || arg[1] == 'c');
        // l:.*
        bool islayout = (arg.size() >= 2 && arg[0] == 'l' && arg[1] == ':');
        bool iscommand = isedit || isnewthing || islayout;
        bool isfile = !iscommand;

        if (arg == "av") {
            autoview = !autoview;
        }
        if (arg == "ap") {
            autoplayer = !autoplayer;
        }
        if (arg == "aw") {
            autowindow = !autowindow;
        }
        if (arg == "ac") {
            autocolormap = !autocolormap;
        }

        if (has_one_sequence) {
            if (arg == "nv" || (autoview && isfile)) {
                view = new View;
                gViews.push_back(view);
            }
            if (arg == "np" || (autoplayer && isfile)) {
                player = new Player;
                gPlayers.push_back(player);
            }
            if (arg == "nw" || (autowindow && isfile)) {
                window = new Window;
                gWindows.push_back(window);
            }
            if (arg == "nc" || (autocolormap && isfile)) {
                colormap = new Colormap;
                gColormaps.push_back(colormap);
            }
        }

        if (isedit) {
            Sequence* seq = *(gSequences.end()-1);
            if (!seq) {
                std::cerr << "invalid usage of e: or E:, it needs a sequence" << std::endl;
                exit(EXIT_FAILURE);
            }
            strncpy(seq->editprog, &arg[2], sizeof(seq->editprog));
            if (arg[0] == 'e') {
                seq->edittype = Sequence::EditType::PLAMBDA;
            } else if (arg[0] == 'E') {
                seq->edittype = Sequence::EditType::GMIC;
            } else {
#ifdef USE_OCTAVE
                seq->edittype = Sequence::EditType::OCTAVE;
#else
                std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
            }
        }

        if (islayout) {
            parseLayout(&arg[2]);
        }

        if (isfile) {
            Sequence* seq = new Sequence;
            gSequences.push_back(seq);

            strncpy(&seq->glob[0], arg.c_str(), seq->glob.capacity());
            seq->loadFilenames();

            seq->view = view;
            seq->player = player;
            seq->colormap = colormap;
            window->sequences.push_back(seq);

            has_one_sequence = true;
        }
    }

    for (auto p : gPlayers) {
        p->reconfigureBounds();
    }
}

int main(int argc, char** argv)
{
    SFMLWindow = new sf::RenderWindow(sf::VideoMode(1024, 720), "VideoProcessingViewer");
    SFMLWindow->setVerticalSyncEnabled(true);
    loadDefaultShaders();

    if (getenv("SCALE")) {
        float scale = atof(getenv("SCALE"));
        if (scale > 0)
            ImGui::GetIO().DisplayFramebufferScale = ImVec2(scale, scale);
    }
    ImGui::SFML::Init(*SFMLWindow);
    ImGui::GetIO().IniFilename = nullptr;
    theme();

    parseArgs(argc, argv);

    if (getenv("WATCH") && getenv("WATCH")[0] == '1') {
        watcher_initialize();
    }

    if (getenv("CACHE") && getenv("CACHE")[0] == '0') {
        useCache = false;
    }

    for (auto seq : gSequences) {
        seq->loadTextureIfNeeded();
        if (!seq->getCurrentImage())
            continue;
        seq->autoScaleAndBias();
        switch (seq->getCurrentImage()->format) {
            case Image::RGBA:
            case Image::RGB:
                seq->colormap->shader = getShader("default");
                break;
            case Image::RG:
                seq->colormap->shader = getShader("opticalFlow");
                break;
            case Image::R:
                seq->colormap->shader = getShader("gray");
                break;
        }
    }

    if (!customLayout.empty()) {
        currentLayout = CUSTOM;
    }

    relayout(true);

    std::thread th(frameloader);

    sf::Clock deltaClock;
    bool hasFocus = true;
    int active = 2;
    while (SFMLWindow->isOpen()) {
        bool current_inactive = true;
        sf::Event event;
        while (SFMLWindow->pollEvent(event)) {
            current_inactive = false;
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                SFMLWindow->close();
            } else if (event.type == sf::Event::Resized) {
                relayout(false);
            } else if(event.type == sf::Event::GainedFocus) {
                hasFocus = true;
            } else if(event.type == sf::Event::LostFocus) {
                hasFocus = false;
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::Q)) {
            SFMLWindow->close();
        }

        sf::Time dt = deltaClock.restart();

        for (auto p : gPlayers) {
            current_inactive &= !p->playing;
        }
        for (auto seq : gSequences) {
            current_inactive &= !seq->force_reupload;
        }
        if (hasFocus) {
            for (int k = 0; k < sf::Keyboard::KeyCount; k++) {
                current_inactive &= !ImGui::IsKeyDown(k);
            }
            for (int m = 0; m < 5; m++) {
                current_inactive &= !ImGui::IsMouseDown(m);
            }
        }

        if (!current_inactive)
            active = 3; // delay between asking a window to close and seeing it closed
        active = std::max(active - 1, 0);
        if (!active) {
            sf::sleep(sf::milliseconds(10));
            continue;
        }

        ImGui::SFML::Update(dt);

        menu();
        for (auto p : gPlayers) {
            p->update();
        }
        for (auto w : gWindows) {
            w->display();
        }

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
        if (ImGui::IsKeyPressed(sf::Keyboard::F11)) {
            Image::flushCache();
            useCache = !useCache;
            printf("cache: %d\n", useCache);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::L)) {
            Layout old = currentLayout;
            if (ImGui::IsKeyDown(sf::Keyboard::LControl)) {
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    currentLayout = (Layout) ((NUM_LAYOUTS + currentLayout - 1) % (NUM_LAYOUTS - customLayout.empty()));
                } else {
                    currentLayout = (Layout) ((currentLayout + 1) % (NUM_LAYOUTS - customLayout.empty()));
                }
            } else if (ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
                currentLayout = FREE;
            }
            if (currentLayout != old) {
                relayout(true);
                printf("current layout: %s\n", layoutNames[currentLayout].c_str());
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyDown(sf::Keyboard::LControl) && ImGui::IsKeyPressed(sf::Keyboard::H)) {
            gShowHud = !gShowHud;
        }

        SFMLWindow->clear();
        ImGui::Render();
        SFMLWindow->display();

        for (auto w : gWindows) {
            w->postRender();
        }
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
    //if (debug) ImGui::ShowTestWindow(&debug);

    bool newShader = false;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Players")) {
            for (auto p : gPlayers) {
                if (ImGui::BeginMenu(p->ID.c_str())) {
                    ImGui::Checkbox("Opened", &p->opened);
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
                    ImGui::Text("%s", s->glob.c_str());

                    if (!s->valid) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "INVALID");
                    }

                    ImGui::Spacing();

                    if (ImGui::CollapsingHeader("Attached player")) {
                        for (auto p : gPlayers) {
                            bool attached = p == s->player;
                            if (ImGui::MenuItem(p->ID.c_str(), 0, attached)) {
                                if (s->player)
                                    s->player->reconfigureBounds();
                                s->player = p;
                                s->player->reconfigureBounds();
                            }
                            if (ImGui::BeginPopupContextItem(p->ID.c_str())) {
                                p->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Attached view")) {
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
                    }
                    if (ImGui::CollapsingHeader("Attached colormap")) {
                        for (auto c : gColormaps) {
                            bool attached = c == s->colormap;
                            if (ImGui::MenuItem(c->ID.c_str(), 0, attached)) {
                                s->colormap = c;
                            }
                            if (ImGui::BeginPopupContextItem(c->ID.c_str())) {
                                c->displaySettings();
                                ImGui::EndPopup();
                            }
                        }
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
                relayout(false);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Colormap")) {
            for (auto c : gColormaps) {
                if (ImGui::BeginMenu(c->ID.c_str())) {
                    c->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New colormap")) {
                Colormap* c = new Colormap;
                gColormaps.push_back(c);
            }

            ImGui::EndMenu();
        }

        ImGui::Text("Layout: %s", layoutNames[currentLayout].c_str());
        ImGui::SameLine(); ImGui::ShowHelpMarker("Use Ctrl+L to cycle between layouts.");
        ImGui::EndMainMenuBar();
    }
}

void theme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.WindowPadding = ImVec2(1, 1);
    style.WindowRounding = 1.f;
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

