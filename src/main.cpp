#include <cmath>
#include <string>
#include <glob.h>
#include <iostream>
#include <cfloat>
#include <algorithm>
#include <map>
#include <thread>
#include <unistd.h> // isatty
#include <fstream>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#ifndef SDL
#include "imgui-SFML.h"
#endif

#ifndef SDL
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>
//#include <SFML/OpenGL.hpp>
#else
#include <SDL2/SDL.h>
#include <imgui_impl_sdl_gl2.h>
#include <GL/gl3w.h>
#endif

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
#include "events.hpp"

#include "cousine_regular.c"

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
float gDisplaySquareZoom;
bool gUseCache;
bool gAsync;
bool gShowHud;
std::array<bool, 9> gShowSVGs;
bool gShowHistogram;
bool gShowMenuBar;
bool gShowWindowBar;
bool gShowImage;
ImVec2 gDefaultSvgOffset;
float gDefaultFramerate;
int gDownsamplingQuality;
int gCacheLimitMB;
bool gPreload;
static bool showHelp = false;
int gActive;
bool quitted = false;

void help();
void menu();
void theme();

void frameloader()
{
    while (!quitted) {
        for (int j = 1; j < 100; j+=10) {
            for (int i = 0; i < j; i++) {
                for (auto s : gSequences) {
                    if (!gUseCache || !gPreload)
                        goto sleep;
                    if (s->valid && s->player) {
                        int frame = s->player->frame + i;
                        if (frame >= s->player->minFrame && frame <= s->player->maxFrame) {
                            s->collection->getImage(frame-1);
                        }
                        if (quitted) {
                            goto end;
                        }
                    }
                }
            }
sleep:
            stopTime(5);
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

        if (isedit && has_one_sequence) {
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

        if (issvg && has_one_sequence) {
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

    if (!gWindows.empty()) {
        gWindows[0]->shouldAskFocus = true;
    }
}

#ifndef SDL
sf::RenderWindow* SFMLWindow;
#include <GL/glew.h>
#endif
int main(int argc, char** argv)
{
    config::load();

    float w = config::get_float("WINDOW_WIDTH");
    float h = config::get_float("WINDOW_HEIGHT");
#ifndef SDL
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antialiasingLevel = 2;
    settings.majorVersion = 2;
    settings.minorVersion = 0;

    SFMLWindow = new sf::RenderWindow(sf::VideoMode(w, h), "vpv", sf::Style::Default, settings);
    SFMLWindow->resetGLStates();
    glewInit();

    SFMLWindow->setVerticalSyncEnabled(true);
    ImGui::SFML::Init(*SFMLWindow);
#else
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    //SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    SDL_Window* window = SDL_CreateWindow("vpv", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    gl3wInit();

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSdlGL2_Init(window);
#endif

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    float scale = config::get_float("SCALE");
    auto font = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(cousine_regular_compressed_data_base85, 13.f*scale, &config);
    font->DisplayOffset.y += 1;
    ImGui::GetIO().IniFilename = nullptr;

    ImGui::GetStyle() = config::get_lua()["setup_theme"]();

    config::load_shaders();

    if (getenv("VPVCMD")) {
        char* argv0 = argv[0];
        const int maxc = 1<<10;
        argc = 0;
        argv = (char**) malloc(sizeof(char*) * maxc);
        argv[argc++] = argv0;
        std::ifstream file;
        file.open (getenv("VPVCMD"));
        assert(file.is_open());

        std::string curword;
        std::string newword;
        while (file >> newword) {
            if (!curword.empty())
                curword += ' ';
            curword += newword;
            bool inside = false;
            for (int i = 0; i < curword.size(); i++) {
                if (curword[i] == '"')
                    inside = !inside;
            }

            if (!inside) {
                curword.erase(std::remove(curword.begin(), curword.end(), '"'), curword.end());
                argv[argc++] = strdup(curword.c_str());
                curword = "";
            }
        }
    }

    if (config::get_bool("WATCH")) {
        watcher_initialize();
    }

    gUseCache = config::get_bool("CACHE");
    gAsync = config::get_bool("ASYNC");
    gShowHud = config::get_bool("SHOW_HUD");
    for (int i = 0, show = config::get_bool("SHOW_SVG"); i < 9; i++)
        gShowSVGs[i] = show;
    gShowMenuBar = config::get_bool("SHOW_MENUBAR");
    gShowHistogram = config::get_bool("SHOW_HISTOGRAM");
    gShowWindowBar = config::get_bool("SHOW_WINDOWBAR");
    gShowImage = true;
    gDefaultFramerate = config::get_float("DEFAULT_FRAMERATE");
    gDownsamplingQuality = config::get_float("DOWNSAMPLING_QUALITY");
    gCacheLimitMB = (float)config::get_lua()["toMB"](config::get_string("CACHE_LIMIT"));
    gPreload = config::get_bool("PRELOAD");
    gDisplaySquareZoom = config::get_float("DISPLAY_SQUARE_ZOOM");

    parseLayout(config::get_string("DEFAULT_LAYOUT"));

    if (argc == 1 && !isatty(0)) {
        char** old = argv;
        argv = new char*[2];
        argv[0] = old[0];
        argv[1] = (char*) "-";
        argc = 2;
    }

    parseArgs(argc, argv);

    for (auto colormap : gColormaps) {
        for (auto seq : gSequences) {
            if (seq->colormap == colormap) {
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
                break;
            }
        }
    }

    gDefaultSvgOffset = ImVec2(config::get_float("SVG_OFFSET_X"),
                               config::get_float("SVG_OFFSET_Y"));

    relayout(true);

    std::thread th(frameloader);

#ifndef SDL
    sf::Clock deltaClock;
#endif
    bool hasFocus = true;
    gActive = 2;
    bool done = false;
    bool firstlayout = true;
    while (!done) {
        bool current_inactive = true;
#ifndef SDL
        sf::Event event;
        while (SFMLWindow->pollEvent(event)) {
            current_inactive = false;
            ImGui::SFML::ProcessEvent(event);
            if (event.type == sf::Event::Closed) {
                done = true;
            } else if (event.type == sf::Event::Resized) {
                relayout(false);
            } else if(event.type == sf::Event::GainedFocus) {
                hasFocus = true;
            } else if(event.type == sf::Event::LostFocus) {
                hasFocus = false;
            }
        }
#else
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            current_inactive = false;
            ImGui_ImplSdlGL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    relayout(false);
                }
            }
        }
#endif

        if (isKeyPressed("q")) {
            done = true;
        }

        watcher_check();

        for (auto p : gPlayers) {
            current_inactive &= !p->playing;
        }
        for (auto seq : gSequences) {
            current_inactive &= !seq->force_reupload;
        }
        if (hasFocus) {
            for (int k = 0; k < 512 /* see definition of io::KeysDown */; k++) {
                current_inactive &= !ImGui::IsKeyDown(k);
            }
            for (int m = 0; m < 5; m++) {
                current_inactive &= !ImGui::IsMouseDown(m);
            }
        }

        current_inactive &= std::abs(ImGui::GetIO().MouseWheel) <= 0 && std::abs(ImGui::GetIO().MouseWheelH) <= 0;

        if (!current_inactive)
            gActive = 3; // delay between asking a window to close and seeing it closed
        gActive = std::max(gActive - 1, 0);
        if (!gActive) {
            stopTime(10);
            continue;
        }

#ifndef SDL
        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(*SFMLWindow, dt);
#else
        ImGui_ImplSdlGL2_NewFrame(window);
#endif

        if (gShowMenuBar)
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

        if (isKeyPressed("F11")) {
            Image::flushCache();
            SVG::flushCache();
            gUseCache = !gUseCache;
            printf("cache: %d\n", gUseCache);
        }

        if (isKeyPressed("l")) {
            if (isKeyDown("control")) {
                if (isKeyDown("shift")) {
                    previousLayout();
                } else {
                    nextLayout();
                }
            } else if (isKeyDown("alt")) {
                freeLayout();
            }
        }

        if (isKeyPressed("h") && isKeyDown("control")) {
            gShowHud = !gShowHud;
        }
        if (isKeyPressed("h") && isKeyDown("shift")) {
            gShowHistogram = !gShowHistogram;
        }

        for (int i = 0; i < 9; i++) {
            char d[2] = {static_cast<char>('1' + i), 0};
            if (isKeyDown("control") && isKeyDown("s") && isKeyPressed(d)) {
                gShowSVGs[i] = !gShowSVGs[i];
            }
        }
        char d[2] = {static_cast<char>('0'), 0};
        if (isKeyDown("control") && isKeyDown("s") && isKeyPressed(d)) {
            gShowImage = !gShowImage;
        }

        if (isKeyDown("control") && isKeyPressed("m")) {
            gShowMenuBar = !gShowMenuBar;
            relayout(false);
        }
        if (isKeyDown("shift") && isKeyPressed("m")) {
            gShowWindowBar = !gShowWindowBar;
            relayout(false);
        }

        if (!isKeyDown("control") && !isKeyDown("shift") && isKeyPressed("h")) {
            showHelp = !showHelp;
        }

        if (showHelp)
            help();

        // this fixes the fact that during the first relayout, we don't know the size of the font
        if (firstlayout) {
            relayout(true);
            firstlayout = false;
        }

#ifndef SDL
        SFMLWindow->clear();
        ImGui::Render();
        SFMLWindow->display();
#else
        ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.00f);
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplSdlGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
#endif

        for (auto w : gWindows) {
            w->postRender();
        }
    }

    quitted = true;
    th.join();

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
#undef CLEAR

#ifndef SDL
    SFMLWindow->close();
    ImGui::SFML::Shutdown();
    delete SFMLWindow;
#else
    ImGui_ImplSdlGL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
#endif
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
        static const char text[] = "SCALE = 1"
            "\nWATCH = false"
            "\nPRELOAD = true"
            "\nCACHE = true"
            "\nCACHE_LIMIT = '2GB'"
            "\nSCREENSHOT = 'screenshot_%%d.png'"
            "\nWINDOW_WIDTH = 1024"
            "\nWINDOW_HEIGHT = 720"
            "\nSHOW_HUD = true"
            "\nSHOW_SVG = true"
            "\nSHOW_MENUBAR = true"
            "\nSHOW_WINDOWBAR = true"
            "\nSHOW_HISTOGRAM = false"
            "\nDEFAULT_LAYOUT = \"grid\""
            "\nAUTOZOOM = true"
            "\nSATURATION = 0.05"
            "\nDEFAULT_FRAMERATE = 30.0"
            "\nDISPLAY_SQUARE_ZOOM = 8"
            "\nDOWNSAMPLING_QUALITY = 1"
            "\nSVG_OFFSET_X = 0"
            "\nSVG_OFFSET_Y = 0"
            "\nASYNC = false";
        ImGui::InputTextMultiline("##text", (char*) text, IM_ARRAYSIZE(text), ImVec2(0,0), ImGuiInputTextFlags_ReadOnly);
        T("The configuration should be written in valid Lua.");
        T("Additional tonemaps can be included in vpv using the user configuration.");
    }

    if (H("Misc.")) {
        B(); T("Setting WATCH to 1 enables the live reload mode. If the image is modified on the disk, then it will be reloaded in vpv so that the newest content will be displayed.");
        B(); T("Setting CACHE to 0 disables the caching of the images. This slows down vpv but also makes it use less RAM.");
        B(); T("SCALE allows to rescale vpv's interface (might be useful for high-density displays).");
        ImGui::Spacing();
        T("Shortcuts");
        B(); T(",: save a screenshot of the focused window's content");
        B(); T("ctrl+m: toggle the display of the menu bar");
        B(); T("shift+m: toggle the display of the windows' title bar");
        B(); T("ctrl+h: toggle the display of the hud");
        B(); T("shift+h: toggle the display of the histogram");
        B(); T("q: quit vpv (but who would want to do that?)");
    }

#undef B
#undef T
#undef H

    ImGui::End();
}

static bool debug = false;

void menu()
{
    if (debug) ImGui::ShowDemoWindow(&debug);

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

        ImGui::Text("Layout: %s", getLayoutName().c_str());
        ImGui::SameLine(); ImGui::ShowHelpMarker("Use Ctrl+L to cycle between layouts.");
        ImGui::EndMainMenuBar();
    }
}

