#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#ifndef WINDOWS
#include <sys/stat.h>
#endif
#include <fstream>
#include <future>
#ifdef _WIN32
#include <locale>
#endif

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <GL/gl3w.h>
#include <SDL.h>
#include <imgui_impl_sdl_gl3.h>
#include <thread_pool.hpp>

#include "Colormap.hpp"
#include "EditGUI.hpp"
#include "Histogram.hpp"
#include "Image.hpp"
#include "ImageCache.hpp"
#include "ImageCollection.hpp"
#include "ImageProvider.hpp"
#include "LoadingThread.hpp"
#include "Player.hpp"
#include "SVG.hpp"
#include "Sequence.hpp"
#include "Shader.hpp"
#include "Terminal.hpp"
#include "View.hpp"
#include "Window.hpp"
#include "collection_expression.hpp"
#include "config.hpp"
#include "dragndrop.hpp"
#include "events.hpp"
#include "globals.hpp"
#include "layout.hpp"
#include "menu.hpp"
#include "shaders.hpp"
#include "strutils.hpp"
#include "watcher.hpp"

#include "cousine_regular.c"

static void help();

static void parseArgs(int argc, char** argv)
{
    if (argc == 1)
        return;
    auto view = newView();
    auto player = newPlayer();
    auto window = newWindow();
    auto colormap = newColormap();

    bool autoview = false;
    bool autoplayer = false;
    bool autowindow = true;
    bool autocolormap = false;
    bool has_one_sequence = false;

    std::map<std::shared_ptr<Sequence>, std::pair<std::string, EditType>> editings;
    std::map<std::shared_ptr<Sequence>, std::vector<std::string>> svgglobs;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // (e:|E:|o:).*
        bool isedit = (arg.size() >= 2 && (arg[0] == 'e' || arg[0] == 'E' || arg[0] == 'o') && arg[1] == ':');
        // t:.*
        bool isterm = arg.size() >= 2 && arg[0] == 't' && arg[1] == ':';
        // (n|a)(v|p|w|c)
        bool isnewthing = arg.size() == 2 && (arg[0] == 'n' || arg[0] == 'a')
            && (arg[1] == 'v' || arg[1] == 'p' || arg[1] == 'w' || arg[1] == 'c');
        // (v|p|c):<num>
        bool isoldthing = arg.size() >= 3 && (arg[0] == 'v' || arg[0] == 'p' || arg[0] == 'c')
            && arg[1] == ':' && atoi(&arg[2]);
        // (v|c|p):.*
        bool isconfig = !isoldthing && (startswith(arg, "v:") || startswith(arg, "c:") || startswith(arg, "p:"));
        // l:.*
        bool islayout = (arg.size() >= 2 && arg[0] == 'l' && arg[1] == ':');
        // svg:.*
        bool issvg = (arg.size() >= 5 && arg[0] == 's' && arg[1] == 'v' && arg[2] == 'g' && arg[3] == ':');
        // shader:.*
        bool isshader = !strncmp(argv[i], "shader:", 7);
        // fromfile:
        bool isfromfile = !strncmp(argv[i], "fromfile:", 9);

        bool iscommand = isedit || isconfig || isnewthing || isoldthing || islayout || issvg || isshader || isterm;
        bool isfile = !iscommand && !isfromfile;
        bool isanewsequence = isfile || isfromfile;

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
            if (arg == "nv" || (autoview && isanewsequence)) {
                view = newView();
            }
            if (arg == "np" || (autoplayer && isanewsequence)) {
                player = newPlayer();
            }
            if (arg == "nw" || (autowindow && isanewsequence)) {
                window = newWindow();
            }
            if (arg == "nc" || (autocolormap && isanewsequence)) {
                colormap = newColormap();
            }
        }

        if (isoldthing) {
            int id = atoi(&arg[2]) - 1;
            id = std::max(0, id);
            if (arg[0] == 'v') {
                id = std::min(id, (int)gViews.size() - 1);
                view = gViews[id];
            } else if (arg[0] == 'p') {
                id = std::min(id, (int)gPlayers.size() - 1);
                player = gPlayers[id];
            } else if (arg[0] == 'c') {
                id = std::min(id, (int)gColormaps.size() - 1);
                colormap = gColormaps[id];
            }
        }

        if (isedit && has_one_sequence) {
            const auto& seq = *(gSequences.end() - 1);
            if (!seq) {
                std::cerr << "invalid usage of e:, E: or o:, it needs a sequence" << std::endl;
                exit(EXIT_FAILURE);
            }
            EditType edittype = PLAMBDA;
            if (arg[0] == 'e') {
#ifdef USE_PLAMBDA
                edittype = EditType::PLAMBDA;
#else
                std::cerr << "plambda isn't enabled, check your compilation." << std::endl;
#endif
            } else if (arg[0] == 'E') {
                std::cerr << "GMIC is no longer supported." << std::endl;
            } else {
#ifdef USE_OCTAVE
                edittype = EditType::OCTAVE;
#else
                std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
            }

            // delay the edit because sequences might not exist yet
            editings[seq] = std::make_pair(arg.substr(2), edittype);
        }

        if (isterm) {
            arg.erase(0, 2);
            gTerminal.bufcommand = arg;
            gTerminal.setVisible(true);
            gTerminal.focusInput = false;
        }

        if (isconfig) {
            if (arg[0] == 'v') {
                view->parseArg(arg);
            } else if (arg[0] == 'c') {
                colormap->parseArg(arg);
            } else if (arg[0] == 'p') {
                player->parseArg(arg);
            }
        }

        if (islayout) {
            parseLayout(&arg[2]);
        }

        if (issvg && has_one_sequence) {
            std::string glob(&argv[i][4]);
            const auto& seq = gSequences[gSequences.size() - 1];
            svgglobs[seq].push_back(glob);
        }

        if (isshader) {
            std::string shader(&argv[i][7]);
            if (!colormap->setShader(shader)) {
                fprintf(stderr, "unknown shader \"%s\"\n", shader.c_str());
            }
        }

        if (isanewsequence) {
            auto seq = newSequence(colormap, player, view);
            std::shared_ptr<ImageCollection> col;
            if (isfromfile) {
                const char* filename = &argv[i][9];
                std::ifstream file(filename);
                if (!file.is_open()) {
                    fprintf(stderr, "could not open fromfile: file '%s', skipped\n", filename);
                    continue;
                }
                std::string fakeglob;
                std::string line;
                while (std::getline(file, line)) {
                    fakeglob += line + SEQUENCE_SEPARATOR;
                }
                if (!fakeglob.empty()) {
                    *(fakeglob.end() - strlen(SEQUENCE_SEPARATOR)) = 0;
                }
                auto filenames = buildFilenamesFromExpression(fakeglob);
                col = buildImageCollectionFromFilenames(filenames);
            } else {
                assert(isfile);
                auto filenames = buildFilenamesFromExpression(argv[i]);
                col = buildImageCollectionFromFilenames(filenames);
            }
            seq->setImageCollection(col, argv[i]);
            window->sequences.push_back(seq);
            has_one_sequence = true;
        }
    }

    for (const auto& seq : gSequences) {
        if (editings.find(seq) != editings.end()) {
            const auto& edit = editings[seq];
            seq->setEdit(edit.first, edit.second);
        }
        if (svgglobs.find(seq) != svgglobs.end()) {
            auto& globs = svgglobs[seq];
            seq->setSVGGlobs(globs);
        }
    }

    if (!gWindows.empty()) {
        gWindows[0]->shouldAskFocus = true;
    }
}

#if defined(__MINGW32__) && defined(main) // SDL is doing weird things
#undef main // this allows to compile on MSYS
#endif

#ifdef WIN32
/** Hello there!
 * You might wonder why this is required.
 * That's because the entry point of the program (before main is called)
 * takes care of filling argc and argv as it wishes.
 * By default, it expands wildcards, even if they were escaped by the shell.
 * In the doc, we can find this: https://docs.microsoft.com/en-us/cpp/c-runtime-library/getmainargs-wgetmainargs
 * but it doesn't say how to disable it.
 * So I found this line in some other place, uhh, I don't know if that is MSYS specific or what.
 * Now... cmd and powershell don't seem to expand * themselves, only bash/fish/etc do.
 * This means that the usual windows user won't be able to do: vpv *.png to open many sequences,
 * instead it will open only one sequence, and vpv \*.png will be invalid in such shells.
 * Not that we can't reconsiliate both world without trying to detect which shell it is from vpv
 * and I don't want to do that. I don't want to change the character '*' to something else either.
 * So if you are a windows user, good luck and maybe try to use bash or equivalent.
 */
int _dowildcard = 0;
#endif

int main(int argc, char* argv[])
{
// This ensures that on Windows, calls to fopen() with path containing UTF8 chars will work.
#ifdef _WIN32
    std::locale::global(std::locale("LC_CTYPE=.UTF8"));
#endif

    bool launched_from_gui = false;
    // on MacOSX, -psn_xxxx is given as argument when launched from GUI
    if (argc >= 2) {
        if (!strncmp("-psn_", argv[1], 5)) {
            for (int i = 1; i < argc - 1; i++) {
                argv[i] = argv[i + 1];
            }
            argc -= 1;
            launched_from_gui = true;
        }
    }

    config::load();

    int w = config::get_int("WINDOW_WIDTH");
    int h = config::get_int("WINDOW_HEIGHT");
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
#ifndef VPV_VERSION
#define VPV_VERSION ""
#endif
    std::string title("vpv " VPV_VERSION);
    SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    if (gl3wInit()) {
        fprintf(stderr, "failed to initialize OpenGL\n");
        return -1;
    }
    if (!gl3wIsSupported(3, 3)) {
        fprintf(stderr, "OpenGL 3.2 not supported\n");
        return -1;
    }

    SDL_PumpEvents();
    SDL_SetWindowSize(window, w, h);

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSdlGL3_Init(window);

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    float scale = config::get_float("SCALE");
    auto font = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(cousine_regular_compressed_data_base85, 13.f * scale, &config);
    font->DisplayOffset.y += 1;
    ImGui::GetIO().IniFilename = nullptr;

    ImGui::GetStyle() = config::get_lua()["setup_theme"]();

    config::load_shaders();

    if (getenv("VPVCMD")) {
        char* argv0 = argv[0];
        const int maxc = 1 << 10;
        argc = 0;
        argv = (char**)malloc(sizeof(char*) * maxc);
        argv[argc++] = argv0;
        const char* filename = getenv("VPVCMD");
        std::ifstream file;
        file.open(filename);
        if (!file.is_open()) {
            fprintf(stderr, "could not open vpvcmd file '%s'\n", filename);
            return 1;
        }

        std::string curword;
        std::string newword;
        while (file >> newword) {
            if (!curword.empty())
                curword += ' ';
            curword += newword;
            bool inside = false;
            for (char i : curword) {
                if (i == '"')
                    inside = !inside;
            }

            if (!inside) {
                curword.erase(std::remove(curword.begin(), curword.end(), '"'), curword.end());
                argv[argc++] = strdup(curword.c_str());
                curword = "";
            }
        }
    }

#ifndef WINDOWS
    if (config::get_bool("WATCH"))
#endif
    {
        watcher_initialize();
    }

    gShowHud = config::get_bool("SHOW_HUD");
    for (int i = 0, show = config::get_bool("SHOW_SVG"); i < 9; i++)
        gShowSVGs[i] = show;
    gShowMenuBar = config::get_bool("SHOW_MENUBAR");
    gShowWindowBar = config::get_int("SHOW_WINDOWBAR");
    gShowHistogram = config::get_bool("SHOW_HISTOGRAM");
    gShowMiniview = config::get_bool("SHOW_MINIVIEW");
    gWindowBorder = config::get_int("WINDOW_BORDER");
    gShowImage = true;
    gDefaultFramerate = config::get_float("DEFAULT_FRAMERATE");
    gDownsamplingQuality = config::get_int("DOWNSAMPLING_QUALITY");
    gCacheLimitMB = config::get_lua()["toMB"](config::get_string("CACHE_LIMIT"));
    gSmoothHistogram = config::get_bool("SMOOTH_HISTOGRAM");
    gForceIioOpen = config::get_bool("FORCE_IIO_OPEN");

    parseLayout(config::get_string("DEFAULT_LAYOUT"));

#ifndef WINDOWS
    if (argc == 1 && !launched_from_gui) {
        struct stat s;
        if (!fstat(0, &s)) {
            if (S_ISFIFO(s.st_mode) || S_ISREG(s.st_mode)) {
                char** old = argv;
                argv = new char*[2];
                argv[0] = old[0];
                argv[1] = (char*)"-";
                argc = 2;
            }
        }
    }
#endif

    parseArgs(argc, argv);

    relayout();

    auto iotask = []() {
        auto& registry = getGlobalImageRegistry();
        while (true) {
            std::pair<std::shared_ptr<ImageProvider>, ImageRegistry::Key> info;
            Q.wait_dequeue(info);
            auto provider = info.first;
            auto key = info.second;

            if (registry.getStatus(key) == ImageRegistry::LOADED) {
                continue;
            }

            printf("asking to load %s?\n", key.c_str());
            if (!registry.getOrSetLoading(key)) {
                printf("%s is already loading\n", key.c_str());
                continue;
            }

            printf("loading %s\n", key.c_str());
            gActive = 20;
            while (!provider->isLoaded()) {
                provider->progress();
            }

            printf("loaded %s\n", key.c_str());
            auto image = provider->getResult();
            registry.putImage(key, image);
            gActive = 20;
        }
    };

    thread_pool tp(std::thread::hardware_concurrency());
    for (int i = 0; i < tp.get_thread_count(); i++) {
        tp.push_task(iotask);
    }

    LoadingThread computethread([]() -> std::shared_ptr<Progressable> {
        if (!gShowHistogram)
            return nullptr;
        for (const auto& w : gWindows) {
            std::shared_ptr<Progressable> provider = w->histogram;
            if (provider && !provider->isLoaded()) {
                return provider;
            }
        }
        for (const auto& seq : gSequences) {
            if (!seq->image)
                continue;
            std::shared_ptr<Progressable> provider = seq->image->histogram;
            if (provider && !provider->isLoaded()) {
                return provider;
            }
        }
        return nullptr;
    });
    computethread.start();

    if (gSequences.empty()) {
        gShowHelp = true;
    }

    bool hasFocus = true;
    gActive = 2;
    bool done = false;
    bool firstlayout = true;
    while (!done) {
        bool current_inactive = true;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            current_inactive = false;
            ImGui_ImplSdlGL3_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    relayout();
                }
            }
#if SDL_VERSION_ATLEAST(2, 0, 5)
            switch (event.type) {
            case SDL_DROPBEGIN:
                break;
            case SDL_DROPTEXT: {
                char* str = event.drop.file;
                handleDragDropEvent(event.drop.file, false);
                SDL_free(str);
                break;
            }
            case SDL_DROPFILE: {
                char* str = event.drop.file;
                handleDragDropEvent(event.drop.file, false);
                SDL_free(str);
                break;
            }
            case SDL_DROPCOMPLETE:
                handleDragDropEvent("", false);
                break;
            }
#endif
        }

        if (!isKeyDown("shift") && !isKeyDown("alt")
            && !isKeyDown("control") && isKeyPressed("q")) {
            done = true;
        }

        watcher_check();

        if (gReloadImages) {
            gReloadImages = false;
            // I don't know yet how to handle editted collection, errors and reload
            // SAD!
            ImageCache::Error::flush();
            for (const auto& seq : gSequences) {
                seq->forgetImage();
            }
            current_inactive = false;
        }

        for (const auto& p : gPlayers) {
            current_inactive &= !p->playing;
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
        current_inactive &= gShowView == 0;

        if (!current_inactive)
            gActive = 3; // delay between asking a window to close and seeing it closed
        gActive = std::max(gActive - 1, 0);
        if (!gActive) {
            stopTime(10);
            continue;
        }

        ImGui_ImplSdlGL3_NewFrame(window);

        auto f = config::get_lua()["on_tick"];
        if (f) {
            f();
        }

        gShowView = std::max(gShowView - 1, 0);
        if (gShowMenuBar)
            menu();
        for (const auto& p : gPlayers) {
            p->update();
        }

        for (const auto& gWindow : gWindows) {
            gWindow->display();
        }

        for (const auto& seq : gSequences) {
            seq->tick();
        }

        if (isKeyPressed("t")) {
            gTerminal.setVisible(!gTerminal.shown);
        }
        gTerminal.tick();

        if (isKeyPressed("F11")) {
            ImageCache::flush();
            SVG::flushCache();
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
            gShowHistogram &= gShowHud;
        }
        if (isKeyPressed("h") && isKeyDown("shift")) {
            gShowHistogram = !gShowHistogram;
            gShowHud |= gShowHistogram;
        }

        for (int i = 0; i < 9; i++) {
            char d[2] = { static_cast<char>('1' + i), 0 };
            if (isKeyDown("control") && isKeyDown("s") && isKeyPressed(d)) {
                gShowSVGs[i] = !gShowSVGs[i];
            }
        }
        char d[2] = { static_cast<char>('0'), 0 };
        if (isKeyDown("control") && isKeyDown("s") && isKeyPressed(d)) {
            gShowImage = !gShowImage;
        }

        if (isKeyDown("control") && isKeyPressed("m")) {
            gShowMenuBar = !gShowMenuBar;
            relayout();
        }
        if (isKeyDown("shift") && isKeyPressed("m")) {
            gShowWindowBar = !gShowWindowBar;
            relayout();
        }

        if (!isKeyDown("control") && !isKeyDown("shift") && isKeyPressed("h")) {
            gShowHelp = !gShowHelp;
        }

        if (gShowHelp) {
            help();

            if (ImGui::Begin("Help")) {
                auto f = config::get_lua()["gui"];
                if (f) {
                    f();
                }
            }
            ImGui::End();
        }

        // this fixes the fact that during the first relayout, we don't know the size of the font
        if (firstlayout) {
            relayout();
            firstlayout = false;
        }

        ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.00f);
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        for (const auto& w : gWindows) {
            w->postRender();
        }
    }

    computethread.stop();

    bool allow_brutal_exit = false;
    auto future_compute = std::async(std::launch::async, [&computethread] { computethread.join(); });
    auto future_terminal = std::async(std::launch::async, [] { gTerminal.stopAllAndJoin(); });
    // If the threads are not joinable within a short amount of time (for instance, if iio/gdal loads a big image),
    // we allow the programm to exit brutally.
    if (future_compute.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout) {
        allow_brutal_exit = true;
    }
    if (future_terminal.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout) {
        allow_brutal_exit = true;
    }
    // TODO: stop io threads
    allow_brutal_exit = true;

    SVG::flushCache();
    ImageCache::flush();

    ImGui_ImplSdlGL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (allow_brutal_exit) {
        std::exit(0);
    }

    return 0;
}

static void help()
{
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin("Help", &gShowHelp, 0)) {
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
        ImGui::TextDisabled("sequence definition (glob, ::)");
        T("Shortcuts");
        B();
        T("!: remove the current image from the sequence");
    }

    if (H("Colormap")) {
        T("A colormap manages the tonemapping method and the brightness (aka bias) and contrast (aka scale) parameters.");
        T("Default tonemaps include: default (RGB), gray, optical flow, jet.\nAdditional tonemaps can be created through the user configuration.");
        T("Command line: use nc (and ac) to create a new colormap, thus setting sequences independent.");
        T("Command line: use shader:<name> to set the tonemap from the command line.");
        ImGui::Spacing();
        T("Shortcuts");
        B();
        T("s/shift+s: cycle through tonemaps");
        B();
        T("a: automatically adjust bias and scale to fit the min/max of an image");
        B();
        T("shift+a: adjust bias/scale by snapping to the nearest 'common' dynamic (eg: 0-1, 0-255, 0-65535)");
        B();
        T("ctrl+a: same as 'a' but with the min/max of the current viewed region");
        B();
        T("alt+a: same as 'a' but with a saturation cut at 5%% by default");
        B();
        T("mouse scroll: adjust the brightness");
        B();
        T("shift+mouse scroll: adjust the contrast");
        B();
        T("shift+mouse motion: adjust the brightness w.r.t the hovered pixel");
        B();
        T("shift+alt+mouse motion: same with color balancing");
    }

    if (H("View")) {
        T("A view manages the zoom and the displacement in the image.");
        T("Command line: use nv (and av) to create a new view, thus setting sequences independent.");
        ImGui::TextDisabled("v:s");
        ImGui::Spacing();
        T("Shortcuts");
        B();
        T("mouse click+motion: move the view");
        B();
        T("ctrl+arrow keys: move the view");
        B();
        T("i: zoom in");
        B();
        T("o: zoom out");
        B();
        T("z+mouse scroll: zoom in/out");
        B();
        T("r: recenter and adjust the zoom so that the image fits the window");
        B();
        T("shift+r: recenter and set the zoom to 1 (1 pixel image = 1 pixel screen)");
    }

    if (H("Player")) {
        T("A player manages the temporal aspect of sequences.");
        T("The player interface (available through the menu bar or via shortcut) includes few extra parameters, such as framerate, loop and bounds.");
        T("The bounds of the player is set to the maximal bounds of the smaller sequence associated with the player.");
        T("Command line: use np (and ap) to create a new player, thus setting sequences independent.");
        ImGui::Spacing();
        T("Shortcuts");
        B();
        T("alt+num: show the nth player interface");
        B();
        T("left/right: show previous/next image in the sequence");
        B();
        T("p: toggle play");
        B();
        T("F8/F9: increase/decrease the framerate");
    }

    if (H("Window and layouts")) {
        T("A window holds one or multiple sequences, and displays one at a time.");
        T("Windows are displayed according to a layout.\nDefault layouts include: grid, fullscreen, horizontal, vertical.");
        T("Command line: use nw (and aw) to create a new window.\nWarning: aw is set by default, so you need to add another one to disable automatic window creation and thus have multiple sequences per window.");
        T("Command line: use l:<layout> to select the layout at startup.\nThe DEFAULT_LAYOUT option can also be used for that.");
        ImGui::Spacing();
        T("Shortcuts");
        B();
        T("num: show the nth window (useful with fullscreen layout)");
        B();
        T("shift+q: close the current window");
        B();
        T("tab/shift+tab: cycle through windows");
        B();
        T("space/backspace: display the next/previous image attached to the window");
        B();
        T("ctrl+l/shift+ctrl+l: cycle through layouts");
    }

    if (H("Edit")) {
        T("An edit program is a small code attached to a sequence.");
        ImGui::TextDisabled("syntax");
        ImGui::TextDisabled("Command line: e:, E:, o:");
        ImGui::TextDisabled("Shortcuts: e, shift+e, ctrl+o");
        T("Supported edit modules:");
        B();
        T("plambda: "
#ifdef USE_PLAMBDA
          "YES"
#else
          "NO"
#endif
        );
        B();
        T("Octave: "
#ifdef USE_OCTAVE
          "YES"
#else
          "NO"
#endif
        );
        B();
        T("Check your compilation options to turn on/off support for edit modules.");
    }

    if (H("SVG")) {
        T("An SVG can be attached to each sequence.");
        T("The actual supported specification is SVG-Tiny (or a subset of that).");
        T("Command line: use svg:filename, svg:<globexpr> or svg:auto to attach an SVG to the last defined sequence.\nauto means that vpv will search an .svg with the same name as the image.\nWith glob and auto, a sequence can be linked to corresponding sequence of SVGs.");
        T("Multiple SVGs can be attached to the same sequence.");
        ImGui::Spacing();
        T("Shortcuts");
        B();
        T("ctrl+s+num: toggle the display of the nth SVG");
    }

    if (H("User configuration")) {
        T("At startup, vpv looks for the files $HOME/.vpvrc and .vpvrc in the current directory.\nSince they are executed in order, the latter overrides settings of the former.");
#ifdef WINDOWS
        T("On Windows, $HOME might not exist but if you use vpv by launching the exe graphically, then you can save your configuration in a file .vpvrc along with vpv.exe. But this does not work when double-clicking a file to open it with vpv. So there is no clear way to do it. For this reason, for now, WATCH is set to 1 because it is a cool feature.");
#endif
        T("Here is the default configuration (might not be up-to-date):");
        static char text[] = "SCALE = 1"
                             "\nWATCH = false"
                             "\nCACHE_LIMIT = '2GB'"
                             "\nSCREENSHOT = 'screenshot_%d.png'"
                             "\nWINDOW_WIDTH = 1024"
                             "\nWINDOW_HEIGHT = 720"
                             "\nSHOW_HUD = true"
                             "\nSHOW_SVG = true"
                             "\nSHOW_MENUBAR = true"
                             "\nSHOW_WINDOWBAR = true"
                             "\nSHOW_HISTOGRAM = false"
                             "\nSHOW_MINIVIEW = true"
                             "\nWINDOW_BORDER = 1"
                             "\nDEFAULT_LAYOUT = \"grid\""
                             "\nSATURATIONS = {0.001, 0.01, 0.1}"
                             "\nDEFAULT_FRAMERATE = 30.0"
                             "\nDOWNSAMPLING_QUALITY = 1"
                             "\nSMOOTH_HISTOGRAM = false"
                             "\nSVG_OFFSET_X = 0"
                             "\nSVG_OFFSET_Y = 0";
        ImGui::InputTextMultiline("##text", text, IM_ARRAYSIZE(text), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
        T("The configuration should be written in valid Lua.");
        T("Additional tonemaps can be included in vpv using the user configuration.");
    }

    if (H("Misc.")) {
        B();
        T("Setting WATCH to 1 enables the live reload mode. If the image is modified on the disk, then it will be reloaded in vpv so that the newest content will be displayed.");
        B();
        T("Setting CACHE to 0 disables the caching of the images. This slows down vpv but also makes it use less RAM.");
        B();
        T("SCALE allows to rescale vpv's interface (might be useful for high-density displays).");
        ImGui::Spacing();
        T("Shortcuts");
        B();
        T(",: save a screenshot of the focused window's content");
        B();
        T("ctrl+m: toggle the display of the menu bar");
        B();
        T("shift+m: toggle the display of the windows' title bar");
        B();
        T("ctrl+h: toggle the display of the hud");
        B();
        T("shift+h: toggle the display of the histogram");
        B();
        T("q: quit vpv (but who would want to do that?)");
    }

#undef B
#undef T
#undef H

    ImGui::End();
}

// declare a function that uses doctest so that
// the related functions in doctests::* will be defined
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define main main2
#include "doctest.h"
#undef main
