#include <cmath>
#include <string>
#include <glob.h>
#include <iostream>
#include <cfloat>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "imgui.h"
#include "imgui-SFML.h"
extern "C" {
#include "iio.h"
}
#include "alphanum.hpp"

sf::RenderWindow* SFMLWindow;

struct View {
    std::string ID;

    float zoom;
    float smallzoomfactor;
    sf::Vector2f center;

    View() {
        static int id = 0;
        id++;
        ID = "View " + std::to_string(id);

        zoom = 1.f;
        smallzoomfactor = 30.f;
    }

    void compute(const sf::Texture& tex, sf::Vector2f& u, sf::Vector2f& v) const {
        float w = tex.getSize().x;
        float h = tex.getSize().y;

        u.x = center.x / w - 1 / (2 * zoom);
        u.y = center.y / h - 1 / (2 * zoom);
        v.x = center.x / w + 1 / (2 * zoom);
        v.y = center.y / h + 1 / (2 * zoom);
    }

    void displaySettings() {
        ImGui::DragFloat("Zoom", &zoom, .01f, 0.1f, 300.f, "%g", 2);
        ImGui::DragFloat("Tooltip zoom factor", &smallzoomfactor, .01f, 0.1f, 300.f, "%g", 2);
        ImGui::DragFloat2("Center", &center.x, 0.f, 100.f);
    }
};

struct Sequence;

struct Player {
    std::string ID;

    int frame;
    int currentMinFrame;
    int currentMaxFrame;
    int minFrame;
    int maxFrame;

    float fps = 30.f;
    bool playing = 0;
    bool looping = 1;

    sf::Clock frameClock;
    sf::Time frameAccumulator;
    bool ticked;

    bool opened;

    Player() {
        static int id = 0;
        id++;
        ID = "Player " + std::to_string(id);

        frame = 1;
        minFrame = 1;
        maxFrame = 10000;
        currentMinFrame = 1;
        currentMaxFrame = maxFrame;
        ticked = false;
        opened = true;
    }

    void update();
    void displaySettings();
    void checkBounds();
    void configureWithSequence(const Sequence& seq);
};

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;
    std::vector<std::string> filenames;
    bool valid;

    sf::Texture texture;
    View* view;
    Player* player;

    Sequence() {
        static int id = 0;
        id++;
        ID = "Sequence " + std::to_string(id);

        view = nullptr;
        player = nullptr;
        valid = false;
        glob.reserve(1024);
        glob_.reserve(1024);
        glob = "";
        glob_ = "";
    }

    void loadFilenames() {
        glob_t res;
        ::glob(glob.c_str(), GLOB_TILDE | GLOB_NOSORT, NULL, &res);
        filenames.resize(res.gl_pathc);
        for(unsigned int j = 0; j < res.gl_pathc; j++) {
            filenames[j] = res.gl_pathv[j];
        }
        globfree(&res);
        std::sort(filenames.begin(), filenames.end(), doj::alphanum_less<std::string>());

        valid = filenames.size() > 0;
        strcpy(&glob_[0], &glob[0]);
        if (player) player->ticked = true;
    }
};

struct Window;

struct WindowMode {
    virtual void display(Window&) = 0;
};

struct FlipWindowMode : WindowMode {
    int index = 0;

    virtual void display(Window&);
};

struct Window {
    std::string ID;
    std::vector<Sequence*> sequences;
    WindowMode* mode;

    Window() {
        static int id = 0;
        id++;
        ID = "Window " + std::to_string(id);

        mode = new FlipWindowMode;
    }

    void display() {
        if (sequences.size()) {
            mode->display(*this);
        }
    }
};

std::vector<Sequence*> sequences;
std::vector<View*> views;
std::vector<Player*> players;
std::vector<Window*> windows;

void player(Player& p);
void menu();
void theme();

void load_textures_if_needed()
{
    for (auto seq : sequences) {
        if (seq->valid && seq->player && seq->player->ticked) {
            int frame = seq->player->frame;
            if (!seq->texture.loadFromFile(seq->filenames[frame - 1])) {
                int w, h, d;
                float* pixels = iio_read_image_float_vec(seq->filenames[frame - 1].c_str(), &w, &h, &d);
                float min = 0;
                float max = FLT_MIN;
                for (int i = 0; i < w*h*d; i++) {
                    min = fminf(min, pixels[i]);
                    max = fmaxf(max, pixels[i]);
                }
                float a = 1.f;
                float b = 0.f;
                if (fabsf(min - 0.f) < 0.01f && fabsf(max - 1.f) < 0.01f) {
                    a = 255.f;
                } else {
                    a = 255.f / (max - min);
                    b = - min;
                }
                unsigned char* rgba = new unsigned char[w * h * 4];
                for (int i = 0; i < w*h; i++) {
                    rgba[i * 4 + 0] = b + a*pixels[i * d + 0];
                    if (d > 1)
                        rgba[i * 4 + 1] = b + a*pixels[i * d + 1];
                    if (d > 2)
                        rgba[i * 4 + 2] = b + a*pixels[i * d + 2];
                    for (int dd = d; dd < 3; dd++) {
                        rgba[i * 4 + dd] = rgba[i * 4];
                    }
                    rgba[i * 4 + 3] = 255;
                }
                seq->texture.create(w, h);
                seq->texture.update(rgba);
                free(pixels);
                delete[] rgba;
            }
        }
    }
}

void parseArgs(int argc, char** argv)
{
    View* view = new View;
    views.push_back(view);

    Player* player = new Player;
    players.push_back(player);

    Window* window = new Window;
    windows.push_back(window);

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "nv") {
            view = new View;
            views.push_back(view);
        } else if (arg == "np") {
            player = new Player;
            players.push_back(player);
        } else if (arg == "nw") {
            window = new Window;
            windows.push_back(window);
        } else {
            Sequence* seq = new Sequence;
            sequences.push_back(seq);

            strncpy(&seq->glob[0], arg.c_str(), seq->glob.capacity());
            seq->loadFilenames();

            seq->texture.setSmooth(false);
            seq->view = view;
            seq->player = player;
            seq->player->configureWithSequence(*seq);
            window->sequences.push_back(seq);
        }
    }
}

int main(int argc, char** argv)
{
    SFMLWindow = new sf::RenderWindow(sf::VideoMode(640, 480), "Video Viewer");
    SFMLWindow->setVerticalSyncEnabled(true);
    ImGui::SFML::Init(*SFMLWindow);
    theme();

    parseArgs(argc, argv);

    load_textures_if_needed();
    for (auto seq : sequences) {
        seq->view->center = ImVec2(seq->texture.getSize().x / 2, seq->texture.getSize().y / 2);
    }

    sf::Clock deltaClock;
    while (SFMLWindow->isOpen()) {
        sf::Event event;
        while (SFMLWindow->pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                SFMLWindow->close();
            }
        }

        ImGui::SFML::Update(deltaClock.restart());

        for (auto p : players) {
            p->update();
        }
        for (auto w : windows) {
            w->display();
        }
        menu();

        load_textures_if_needed();

        SFMLWindow->clear();
        ImGui::Render();
        SFMLWindow->display();
    }

    ImGui::SFML::Shutdown();
    delete SFMLWindow;
}


void Player::update()
{
    frameAccumulator += frameClock.restart();

    int oldframe = frame;
    ticked = false;

    if (playing) {
        while (frameAccumulator.asSeconds() > 1 / fabsf(fps)) {
            frame += fps >= 0 ? 1 : -1;
            frameAccumulator -= sf::seconds(1 / fabsf(fps));
            ticked = true;
            checkBounds();
        }
    }

    if (!opened) {
        return;
    }

    if (!ImGui::Begin(("Player###" + ID).c_str(), &opened, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    displaySettings();

    checkBounds();

    ImGui::End();
}

void Player::displaySettings()
{
    if (ImGui::Button("<")) {
        frame--;
        ticked = true;
        playing = 0;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Play", &playing)) {
        frameAccumulator = sf::seconds(0);
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        frame++;
        ticked = true;
        playing = 0;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Looping", &looping);
    if (ImGui::SliderInt("Frame", &frame, currentMinFrame, currentMaxFrame)) {
        ticked = true;
        playing = 0;
    }
    ImGui::SliderFloat("FPS", &fps, -100.f, 100.f, "%.2f frames/s");
    //ImGui::SliderInt("Min frame", &currentMinFrame, minFrame, maxFrame);
    //ImGui::SliderInt("Max frame", &currentMaxFrame, minFrame, maxFrame);
    ImGui::DragIntRange2("Bounds", &currentMinFrame, &currentMaxFrame, 1.f, minFrame, maxFrame);
}

void Player::checkBounds()
{
    currentMaxFrame = fmin(currentMaxFrame, maxFrame);
    currentMinFrame = fmax(currentMinFrame, minFrame);
    currentMinFrame = fmin(currentMinFrame, currentMaxFrame);

    if (frame > currentMaxFrame) {
        if (looping)
            frame = currentMinFrame;
        else
            frame = currentMaxFrame;
    }
    if (frame < currentMinFrame) {
        if (looping)
            frame = currentMaxFrame;
        else
            frame = currentMinFrame;
    }
}

void Player::configureWithSequence(const Sequence& seq)
{
    maxFrame = fmin(maxFrame, seq.filenames.size());

    ticked = true;
    checkBounds();
}

struct CustomConstraints {
    static void AspectRatio(ImGuiSizeConstraintCallbackData* data) {
        sf::Texture* tex = (sf::Texture*) data->UserData;
        float aspect = (float) tex->getSize().x / tex->getSize().y;
        float w = data->DesiredSize.x;
        float h = w / aspect;
        data->DesiredSize.x = w;
        data->DesiredSize.y = h;
    }
};

void FlipWindowMode::display(Window& window)
{
    Sequence& seq = *window.sequences[index];
    View* view = seq.view;
    sf::Texture& texture = seq.texture;

    if (!seq.valid || !seq.player) {
        return;
    }

    ImGui::SetNextWindowSize((ImVec2) texture.getSize(), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(32, 32), ImVec2(FLT_MAX, FLT_MAX), CustomConstraints::AspectRatio, &texture);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", seq.filenames[seq.player->frame - 1].c_str(), window.ID.c_str());
    ImGui::Begin(buf, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

    sf::Vector2f u, v;
    view->compute(texture, u, v);
    ImGui::Image(&texture, ImGui::GetContentRegionAvail(), u, v);

    if (ImGui::IsWindowFocused()) {
        if (!ImGui::IsMouseDown(2) && ImGui::GetIO().MouseWheel != 0.f) {
            view->zoom *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
        }

        ImVec2 drag = ImGui::GetMouseDragDelta(1);
        if (drag.x || drag.y) {
            ImGui::ResetMouseDragDelta(1);
            view->center -= (sf::Vector2f) drag / view->zoom;
        }

        if (ImGui::IsMouseDown(2)) {
            if (ImGui::GetIO().MouseWheel != 0.f) {
                view->smallzoomfactor *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
            }
            sf::Vector2f mousePos = (sf::Vector2f) ImGui::GetMousePos() - (sf::Vector2f) ImGui::GetWindowPos();
            float texw = (float) texture.getSize().x;
            float texh = (float) texture.getSize().y;
            float cx = view->center.x;
            float cy = view->center.y;

            float rx = mousePos.x / ImGui::GetWindowSize().x;
            float ry = mousePos.y / ImGui::GetWindowSize().y;

            ImGui::BeginTooltip();
            {
                sf::Vector2f uu, vv;
                uu.x = u.x * (1.f - rx) + v.x * rx - 1 / (2 * view->zoom*view->smallzoomfactor);
                uu.y = u.y * (1.f - ry) + v.y * ry - 1 / (2 * view->zoom*view->smallzoomfactor);
                vv.x = u.x * (1.f - rx) + v.x * rx + 1 / (2 * view->zoom*view->smallzoomfactor);
                vv.y = u.y * (1.f - ry) + v.y * ry + 1 / (2 * view->zoom*view->smallzoomfactor);

                ImGui::Image(&texture, ImVec2(128, 128*texh/texw), uu, vv);
                ImGui::Text("around (%.0f, %.0f)", (uu.x+vv.x)/2*texw, (uu.y+vv.y)/2*texh);
            }
            ImGui::EndTooltip();
        }

        if (ImGui::IsKeyPressed(sf::Keyboard::Space)) {
            index = (index + 1) % window.sequences.size();
        }
    }

    ImGui::End();
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
            for (auto p : players) {
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
                players.push_back(p);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Sequences")) {
            for (auto s : sequences) {
                if (ImGui::BeginMenu(s->ID.c_str())) {
                    ImGui::Text(s->glob.c_str());

                    if (!s->valid) {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "INVALID");
                    }

                    ImGui::Spacing();

                    if (ImGui::BeginMenu("Attached player")) {
                        for (auto p : players) {
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
                        for (auto v : views) {
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
                    if (ImGui::InputText("File glob", &s->glob_[0], s->glob_.capacity(), ImGuiInputTextFlags_EnterReturnsTrue)) {
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
                s->view = views[0];
                sequences.push_back(s);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Views")) {
            for (auto v : views) {
                if (ImGui::BeginMenu(v->ID.c_str())) {
                    v->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New view")) {
                View* v = new View;
                views.push_back(v);
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Debug", nullptr, &debug)) debug = true;

        ImGui::EndMainMenuBar();
    }
}

void theme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
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
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
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

