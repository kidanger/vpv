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
#include "SVG.hpp"
#include "globals.hpp"
#include "shaders.hpp"
#include "layout.hpp"
#include "watcher.hpp"
#include "config.hpp"

sf::RenderWindow* SFMLWindow;

std::vector<Sequence*> gSequences;
std::vector<View*> gViews;
std::vector<Player*> gPlayers;
std::vector<Window*> gWindows;
std::vector<Colormap*> gColormaps;
std::vector<Shader*> gShaders;
bool gSelecting;
ImVec2 gSelectionFrom;
ImVec2 gSelectionTo;
bool gSelectionShown;
ImVec2 gHoveredPixel;
bool gUseCache;
bool gAsync;
bool gShowHud;
std::array<bool, 9> gShowSVGs;
bool gShowMenu;
bool gShowImage;
ImVec2 gDefaultSvgOffset;
float gDefaultFramerate;
static bool showHelp = false;
int gActive;

void help();
void menu();
void theme();

void frameloader()
{
    while (SFMLWindow->isOpen()) {
        for (int j = 1; j < 100; j+=10) {
            for (int i = 0; i < j; i++) {
                for (auto s : gSequences) {
                    if (!gUseCache)
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
    bool autowindow = true;
    bool autocolormap = false;
    bool has_one_sequence = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // (e:|E:|o:).*
        bool isedit = (arg.size() >= 2 && (arg[0] == 'e' || arg[0] == 'E' || arg[0] == 'o') && arg[1] == ':');
        // (v:).*
        bool isconfig = (arg.size() >= 2 && (arg[0] == 'v') && arg[1] == ':');
        // (n|a)(v|p|w|c)
        bool isnewthing = arg.size() == 2 && (arg[0] == 'n' || arg[0] == 'a')
                        && (arg[1] == 'v' || arg[1] == 'p' || arg[1] == 'w' || arg[1] == 'c');
        // l:.*
        bool islayout = (arg.size() >= 2 && arg[0] == 'l' && arg[1] == ':');
        // svg:.*
        bool issvg = (arg.size() >= 5 && arg[0] == 's' && arg[1] == 'v' && arg[2] == 'g' && arg[3] == ':');
        // shader:.*
        bool isshader = !strncmp(argv[i], "shader:", 7);
        bool iscommand = isedit || isconfig || isnewthing || islayout || issvg || isshader;
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

        if (isedit && !gSequences.empty()) {
            Sequence* seq = *(gSequences.end()-1);
            if (!seq) {
                std::cerr << "invalid usage of e: or E:, it needs a sequence" << std::endl;
                exit(EXIT_FAILURE);
            }
            strncpy(seq->editprog, &arg[2], sizeof(seq->editprog));
            if (arg[0] == 'e') {
                seq->edittype = EditType::PLAMBDA;
            } else if (arg[0] == 'E') {
#ifdef USE_GMIC
                seq->edittype = EditType::GMIC;
#else
                std::cerr << "GMIC isn't enabled, check your compilation." << std::endl;
#endif
            } else {
#ifdef USE_OCTAVE
                seq->edittype = EditType::OCTAVE;
#else
                std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
            }
        }

        if (isconfig) {
            if (arg[0] == 'v') {
                if (arg[2] == 's') {
                    view->shouldRescale = true;
                }
            }
        }

        if (islayout) {
            parseLayout(&arg[2]);
        }

        if (issvg && !gSequences.empty()) {
            std::string glob(&argv[i][4]);
            Sequence* seq = gSequences[gSequences.size()-1];
            seq->svgglobs.push_back(glob);
        }

        if (isshader) {
            std::string shader(&argv[i][7]);
            Shader* s = getShader(shader);
            if (s) {
                colormap->shader = s;
            } else {
                fprintf(stderr, "unknown shader \"%s\"\n", shader.c_str());
            }
        }

        if (isfile) {
            Sequence* seq = new Sequence;
            gSequences.push_back(seq);

            strncpy(&seq->glob[0], argv[i], seq->glob.capacity());

            seq->view = view;
            seq->player = player;
            seq->colormap = colormap;
            window->sequences.push_back(seq);

            has_one_sequence = true;
        }
    }

    for (auto seq : gSequences) {
        seq->loadFilenames();
    }

    for (auto p : gPlayers) {
        p->reconfigureBounds();
    }
}

int main(int argc, char** argv)
{
    config::load();

    float w = config::get_float("WINDOW_WIDTH");
    float h = config::get_float("WINDOW_HEIGHT");
    SFMLWindow = new sf::RenderWindow(sf::VideoMode(w, h), "vpv");
    SFMLWindow->setVerticalSyncEnabled(true);
    config::load_shaders();

    float scale = config::get_float("SCALE");
    ImGui::GetIO().DisplayFramebufferScale = ImVec2(scale, scale);
    ImGui::SFML::Init(*SFMLWindow);
    ImGui::GetIO().IniFilename = nullptr;
    theme();

    if (config::get_bool("WATCH")) {
        watcher_initialize();
    }

    gUseCache = config::get_bool("CACHE");
    gAsync = config::get_bool("ASYNC");
    gShowHud = config::get_bool("SHOW_HUD");
    for (int i = 0, show = config::get_bool("SHOW_SVG"); i < 9; i++)
        gShowSVGs[i] = show;
    gShowMenu = config::get_bool("SHOW_MENUBAR");
    gShowImage = true;
    gDefaultFramerate = config::get_float("DEFAULT_FRAMERATE");

    parseLayout(config::get_string("DEFAULT_LAYOUT"));

    parseArgs(argc, argv);

    for (auto seq : gSequences) {
        seq->loadTextureIfNeeded();
        if (!seq->getCurrentImage(false, true))
            continue;
        seq->autoScaleAndBias();
        if (seq->colormap->shader)
            continue; // shader was overridden in command line
        switch (seq->getCurrentImage()->format) {
            case Image::R:
                seq->colormap->shader = getShader("gray");
                break;
            case Image::RG:
                seq->colormap->shader = getShader("opticalFlow");
                break;
            default:
            case Image::RGBA:
            case Image::RGB:
                seq->colormap->shader = getShader("default");
                break;
        }
    }

    gDefaultSvgOffset = ImVec2(config::get_float("SVG_OFFSET_X"),
                               config::get_float("SVG_OFFSET_Y"));

    relayout(true);

    std::thread th(frameloader);

    sf::Clock deltaClock;
    bool hasFocus = true;
    gActive = 2;
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

        watcher_check();

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
            gActive = 3; // delay between asking a window to close and seeing it closed
        gActive = std::max(gActive - 1, 0);
        if (!gActive) {
            sf::sleep(sf::milliseconds(10));
            continue;
        }

        ImGui::SFML::Update(dt);

        if (gShowMenu)
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
            SVG::flushCache();
            gUseCache = !gUseCache;
            printf("cache: %d\n", gUseCache);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::L)) {
            Layout old = currentLayout;
            if (ImGui::IsKeyDown(sf::Keyboard::LControl)) {
                if (ImGui::IsKeyDown(sf::Keyboard::LShift)) {
                    currentLayout = (Layout) ((NUM_LAYOUTS + currentLayout - 2) % (NUM_LAYOUTS - customLayout.empty()));
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

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyDown(sf::Keyboard::LControl)
            && ImGui::IsKeyPressed(sf::Keyboard::H)) {
            gShowHud = !gShowHud;
        }

        for (int i = 0; i < 9; i++) {
            if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyDown(sf::Keyboard::LControl)
                && ImGui::IsKeyDown(sf::Keyboard::S)
                && ImGui::IsKeyPressed(sf::Keyboard::Num1 + i)) {
                gShowSVGs[i] = !gShowSVGs[i];
            }
        }
        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyDown(sf::Keyboard::LControl)
            && ImGui::IsKeyDown(sf::Keyboard::S)
            && ImGui::IsKeyPressed(sf::Keyboard::Num0)) {
            gShowImage = !gShowImage;
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyDown(sf::Keyboard::LControl)
            && ImGui::IsKeyPressed(sf::Keyboard::M)) {
            gShowMenu = !gShowMenu;
            relayout(false);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::IsKeyDown(sf::Keyboard::LControl)
            && ImGui::IsKeyPressed(sf::Keyboard::H))
            showHelp = !showHelp;

        if (showHelp)
            help();

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

#define CLEAR(tab) \
    for (auto s : tab) \
        delete s; \
    tab.clear();
    CLEAR(gSequences);
    CLEAR(gViews);
    CLEAR(gPlayers);
    CLEAR(gWindows);
    CLEAR(gColormaps);
    CLEAR(gShaders);
    SVG::flushCache();
    Image::flushCache();
}

void help()
{
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin("Help", &showHelp, 0))
    {
        ImGui::End();
        return;
    }
    ImGui::BringFront();
    ImGui::TextWrapped("Welcome to vpv's help! Click on the topic you're interested in.");
    ImGui::Spacing();

#define B ImGui::Bullet
#define T ImGui::TextWrapped
#define H ImGui::CollapsingHeader

    if (H("Sequence")) {
        T("A sequence is an ordered collection of images");
        T("Each sequence has a colormap, a view and a player.\nThose objects can be shared by multiple sequences.");
        T("A sequence is displayed on a window.");
        ImGui::TextDisabled("sequence definition (glob, :)");
    }

    if (H("Colormap")) {
        T("A colormap manages the tonemapping method and the brightness (aka bias) and contrast (aka scale) parameters.");
        T("Default tonemaps include: default (RGB), gray, optical flow, jet.\nAdditional tonemaps can be created through the user configuration.");
        T("Command line: use nc (and ac) to create a new colormap, thus setting sequences independent.");
        T("Command line: use shader:<name> to set the tonemap from the command line.");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T("s/shift+s: cycle through tonemaps");
        B(); T("a: automatically adjust bias and scale to fit the min/max of an image");
        B(); T("shift+a: adjust bias/scale by snapping to the nearest 'common' dynamic (eg: 0-1, 0-255, 0-65535)");
        B(); T("ctrl+a: same as 'a' but with the min/max of the current viewed region");
        B(); T("alt+a: same as 'a' but with a saturation cut at 5%% by default");
        B(); T("mouse scroll: adjust the brightness");
        B(); T("shift+mouse scroll: adjust the contrast");
        B(); T("shift+mouse motion: adjust the brightness w.r.t the hovered pixel");
        B(); T("shift+alt+mouse motion: same with color balancing");
    }

    if (H("View")) {
        T("A view manages the zoom and the displacement in the image.");
        T("Command line: use nv (and av) to create a new view, thus setting sequences independent.");
        ImGui::TextDisabled("v:s");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T("mouse click+motion: move the view");
        B(); T("i: zoom in");
        B(); T("o: zoom out");
        B(); T("z+mouse scroll: zoom in/out");
        B(); T("r: recenter and adjust the zoom so that the image fits the window");
        B(); T("shift+r: recenter and set the zoom to 1 (1 pixel image = 1 pixel screen)");
    }

    if (H("Player")) {
        T("A player manages the temporal aspect of sequences.");
        T("The player interface (available through the menu bar or via shortcut) includes few extra parameters, such as framerate, loop and bounds.");
        T("The bounds of the player is set to the maximal bounds of the smaller sequence associated with the player.");
        T("Command line: use np (and ap) to create a new player, thus setting sequences independent.");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T("alt+num: show the nth player interface");
        B(); T("left/right: show previous/next image in the sequence");
        B(); T("p: toggle play");
        B(); T("F8/F9: increase/decrease the framerate");
    }

    if (H("Window and layouts")) {
        T("A window holds one or multiple sequences, and displays one at a time.");
        T("Windows are displayed according to a layout.\nDefault layouts include: grid, fullscreen, horizontal, vertical.");
        T("Command line: use nw (and aw) to create a new window.\nWarning: aw is set by default, so you need to add another one to disable automatic window creation and thus have multiple sequences per window.");
        T("Command line: use l:<layout> to select the layout at startup.\nThe DEFAULT_LAYOUT option can also be used for that.");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T("num: show the nth window (useful with fullscreen layout)");
        B(); T("tab/shift+tab: cycle through windows");
        B(); T("space/backspace: display the next/previous image attached to the window");
        B(); T("ctrl+l/shift+ctrl+l: cycle through layouts");
    }

    if (H("Edit")) {
        T("An edit program is a small code attached to a sequence.");
        ImGui::TextDisabled("syntax");
        ImGui::TextDisabled("Command line: e:, E:, o:");
        ImGui::TextDisabled("Shortcuts: e, shift+e, ctrl+o");
        T("Supported edit modules:");
        B(); T("plambda: YES");
        B(); T("GMIC: "
#ifdef USE_GMIC
               "YES"
#else
               "NO"
#endif
               );
        B(); T("Octave: "
#ifdef USE_OCTAVE
               "YES"
#else
               "NO"
#endif
               );
        B(); T("Check your compilation options to turn on/off support for edit modules.");
    }

    if (H("SVG")) {
        T("An SVG can be attached to each sequence.");
        T("The actual supported specification is SVG-Tiny (or a subset of that).");
        T("Command line: use svg:filename, svg:glob or svg:auto to attach an SVG to the last defined sequence.\nauto means that vpv will search an .svg with the same name as the image.\nWith glob and auto, a sequence can be linked to corresponding sequence of SVGs.");
        T("Multiple SVGs can be attached to the same sequence.");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T("ctrl+s+num: toggle the display of the nth SVG");
    }

    if (H("User configuration")) {
        T("At startup, vpv looks for the files $HOME/.vpvrc and .vpvrc in the current directory.\nSince they are executed in order, the latter overrides settings of the former.");
        T("Here is the default configuration (might not be up-to-date):");
        static const char* text = "SCALE = 1"
            "\nWATCH = false"
            "\nCACHE = true"
            "\nSCREENSHOT = 'screenshot_%%d.png'"
            "\nWINDOW_WIDTH = 1024"
            "\nWINDOW_HEIGHT = 720"
            "\nSHOW_HUD = true"
            "\nSHOW_SVG = true"
            "\nSHOW_MENUBAR = true"
            "\nDEFAULT_LAYOUT = \"grid\""
            "\nAUTOZOOM = true"
            "\nSATURATION = 0.05"
            "\nDEFAULT_FRAMERATE = 30.0"
            "\nSVG_OFFSET_X = 0"
            "\nSVG_OFFSET_Y = 0"
            "\nASYNC = false";
        ImGui::InputTextMultiline("##text", (char*) text, sizeof(text), ImVec2(0,0), ImGuiInputTextFlags_ReadOnly);
        T("The configuration should be written in valid Lua.");
        T("Additional tonemaps can be included in vpv using the user configuration.");
    }

    if (H("Misc.")) {
        B(); T("Setting WATCH to 1 enables the live reload mode. If the image is modified on the disk, then it will be reloaded in vpv so that the newest content will be displayed.");
        B(); T("Setting CACHE to 0 disabled the caching of the images. This slows down vpv but also makes it use less RAM.");
        B(); T("SCALE allows to rescale vpv's interface (might be useful for high-density displays).");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T(",: save a screenshot of the focused window's content");
        B(); T("ctrl+m: toggle the display of the menu bar");
        B(); T("ctrl+h: toggle the display of the hud");
        B(); T("q: quit vpv (but who would want to do that?)");
    }

#undef B
#undef T
#undef H

    ImGui::End();
}

namespace ImGui {
    void ShowTestWindow(bool* p_open);
}

static bool debug = false;

void menu()
{
    //if (debug) ImGui::ShowTestWindow(&debug);

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

