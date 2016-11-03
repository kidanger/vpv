#include <cmath>
#include <string>
#include <glob.h>
#include <iostream>
#include <cfloat>
#include <algorithm>
#include <map>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
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
    ImVec2 center;

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
    bool visible;
    int loadedFrame;

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
        visible = false;
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

        loadedFrame = -1;
    }
};

struct Window;

struct WindowMode {
    std::string type;

    WindowMode(std::string type) : type(type) {
    }
    virtual ~WindowMode() {}
    virtual void display(Window&) = 0;
    virtual void displaySettings(Window&) {
    }
    virtual void onAddSequence(Window&, Sequence*) {
    }
    virtual const std::string& getTitle(const Window& window) = 0;
};

struct FlipWindowMode : WindowMode {
    int index = 0;

    FlipWindowMode() : WindowMode("Flip") {
    }
    virtual ~FlipWindowMode() {}
    virtual void display(Window&);
    virtual void displaySettings(Window&);
    virtual void onAddSequence(Window&, Sequence*);
    virtual const std::string& getTitle(const Window& window);
};

struct Window {
    std::string ID;
    std::vector<Sequence*> sequences;
    WindowMode* mode;
    bool opened;

    Window() {
        static int id = 0;
        id++;
        ID = "Window " + std::to_string(id);

        mode = new FlipWindowMode;
        opened = true;
    }

    void display();

    void displaySettings();

    void setMode(WindowMode* mode) {
        delete this->mode;
        this->mode = mode;
    }

    void fullscreen(bool next=false, ImGuiSetCond cond=ImGuiSetCond_Always) {
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
};

std::vector<Sequence*> sequences;
std::vector<View*> views;
std::vector<Player*> players;
std::vector<Window*> windows;

void player(Player& p);
void menu();
ImRect getRenderingRect(const sf::Texture& texture, ImRect* windowRect=0);
void theme();

void load_textures_if_needed()
{
    for (auto seq : sequences) {
        if (seq->valid && seq->visible && seq->player && seq->loadedFrame != seq->player->frame) {
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
            seq->loadedFrame = frame;
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
            window->mode->onAddSequence(*window, seq);
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
        if (seq->view->center.x == 0 && seq->view->center.y == 0) {
            seq->view->center = seq->texture.getSize() / 2;
        }
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

        for (auto w : windows) {
            w->display();
        }
        for (auto p : players) {
            p->update();
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

    if (playing) {
        while (frameAccumulator.asSeconds() > 1 / fabsf(fps)) {
            frame += fps >= 0 ? 1 : -1;
            frameAccumulator -= sf::seconds(1 / fabsf(fps));
            checkBounds();
        }
    }

    if (!opened) {
        return;
    }

    if (!ImGui::Begin(ID.c_str(), &opened, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    int index = std::find(players.begin(), players.end(), this) - players.begin();
    if (index <= 9 && ImGui::IsKeyPressed(sf::Keyboard::Num1 + index)
        && ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
        ImGui::SetWindowFocus();
    }

    displaySettings();

    checkBounds();

    ImGui::End();
}

void Player::displaySettings()
{
    if (ImGui::Button("<")) {
        frame--;
        playing = 0;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Play", &playing)) {
        frameAccumulator = sf::seconds(0);
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        frame++;
        playing = 0;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Looping", &looping);
    if (ImGui::SliderInt("Frame", &frame, currentMinFrame, currentMaxFrame)) {
        playing = 0;
    }
    ImGui::SliderFloat("FPS", &fps, -100.f, 100.f, "%.2f frames/s");
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

    checkBounds();
}

void Window::display() {
    if (!opened) {
        return;
    }

    Sequence& seq = *sequences[0];
    sf::Texture& texture = seq.texture;
    View* view = seq.view;

    if (!seq.valid || !seq.player) {
        return;
    }

    if (windows.size() > 1) {
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

    int index = std::find(windows.begin(), windows.end(), this) - windows.begin();
    if (index <= 9 && ImGui::IsKeyPressed(sf::Keyboard::Num1 + index)
        && !ImGui::IsKeyDown(sf::Keyboard::LAlt)) {
        ImGui::SetWindowFocus();
    }

    if (sequences.size()) {
        mode->display(*this);

        if (ImGui::IsWindowFocused()) {
            if (!ImGui::IsMouseDown(2) && ImGui::GetIO().MouseWheel != 0.f) {
                view->zoom *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
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

                sf::Vector2f u, v;
                view->compute(texture, u, v);

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
        }
    }

    ImGui::End();
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

    sf::Vector2f u, v;
    view->compute(texture, u, v);

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

void Window::displaySettings()
{
    ImGui::Text("Sequences");
    ImGui::BeginChild("scrolling", ImVec2(350, ImGui::GetItemsLineHeightWithSpacing()*3 + 20),
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto seq : ::sequences) {
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

        if (ImGui::BeginMenu("Windows")) {
            for (auto w : windows) {
                if (ImGui::BeginMenu(w->ID.c_str())) {
                    w->displaySettings();
                    ImGui::EndMenu();
                }
            }

            ImGui::Spacing();
            if (ImGui::MenuItem("New window")) {
                Window* w = new Window;
                windows.push_back(w);
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Debug", nullptr, &debug)) debug = true;

        ImGui::EndMainMenuBar();
    }
}

ImRect getRenderingRect(const sf::Texture& texture, ImRect* windowRect) {
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

