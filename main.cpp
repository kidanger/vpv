#include <cmath>
#include <string>
#include <glob.h>
#include <iostream>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "imgui.h"
#include "imgui-SFML.h"

sf::RenderWindow* window;

struct View {
    float zoom;
    float smallzoomfactor;
    sf::Vector2f center;

    void compute(const sf::Texture& tex, sf::Vector2f& u, sf::Vector2f& v) const {
        float w = tex.getSize().x;
        float h = tex.getSize().y;

        u.x = center.x / w - 1 / (2 * zoom);
        u.y = center.y / h - 1 / (2 * zoom);
        v.x = center.x / w + 1 / (2 * zoom);
        v.y = center.y / h + 1 / (2 * zoom);
    }
};

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

    void update();
};

struct Sequence {
    std::string glob;
    std::vector<std::string> filenames;
    sf::Texture texture;
    View* view;
    Player* player;
};

std::vector<Sequence> seqs;
std::vector<View> views;
std::vector<Player> players;

void player(Player& p);
void display_sequences();
void theme();

void load_textures_if_needed()
{
    for (auto& seq : seqs) {
        if (seq.player->ticked) {
            int frame = seq.player->frame;
            seq.texture.loadFromFile(seq.filenames[frame - 1]);
        }
    }
}

int main(int argc, char** argv)
{
    window = new sf::RenderWindow(sf::VideoMode(640, 480), "Video Viewer");
    window->setVerticalSyncEnabled(true);
    ImGui::SFML::Init(*window);
    theme();

    views.push_back(View());
    players.push_back(Player());
    players[0].maxFrame = 1000; // FIXME
    players[0].ID = "Player 0";

    seqs.resize(argc - 1);
    for (int i = 0; i < argc - 1; i++) {
        seqs[i].glob = argv[i + 1];
        glob_t res;
        glob(argv[i + 1], GLOB_TILDE, NULL, &res);
        seqs[i].filenames.resize(res.gl_pathc);
        for(unsigned int j = 0; j < res.gl_pathc; j++) {
            seqs[i].filenames[j] = res.gl_pathv[j];
        }
        globfree(&res);

        players[0].maxFrame = fmin(players[0].maxFrame, seqs[i].filenames.size());

        seqs[i].texture.setSmooth(false);
        seqs[i].view = &views[0];
        seqs[i].player = &players[0];
    }

    players[0].frame = 1;
    players[0].minFrame = 1;
    players[0].currentMinFrame = players[0].minFrame;
    players[0].currentMaxFrame = players[0].maxFrame;
    players[0].ticked = true;
    players[0].opened = true;

    load_textures_if_needed();

    views[0].zoom = 1.f;
    views[0].smallzoomfactor = 30.f;
    views[0].center = ImVec2(seqs[0].texture.getSize().x / 2, seqs[0].texture.getSize().y / 2);

    sf::Clock deltaClock;
    while (window->isOpen()) {
        sf::Event event;
        while (window->pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                window->close();
            }
        }

        ImGui::SFML::Update(deltaClock.restart());

        display_sequences();
        for (auto& p : players) {
            p.update();
        }

        load_textures_if_needed();

        window->clear();
        ImGui::Render();
        window->display();
    }

    ImGui::SFML::Shutdown();
    delete window;
}

void Player::update()
{
    frameAccumulator += frameClock.restart();

    int oldframe = frame;
    ticked = false;

    ImGui::Begin(("Player###" + ID).c_str(), &opened, ImGuiWindowFlags_AlwaysAutoResize);
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
    ImGui::SliderInt("Min frame", &currentMinFrame, minFrame, maxFrame);
    ImGui::SliderInt("Max frame", &currentMaxFrame, minFrame, maxFrame);

    if (playing) {
        while (frameAccumulator.asSeconds() > 1 / fabsf(fps)) {
            frame += fps >= 0 ? 1 : -1;
            frameAccumulator -= sf::seconds(1 / fabsf(fps));
        }
    }

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

    if (frame != oldframe) {
        ticked = true;
    }

    ImGui::End();
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

void display_sequences()
{
    for (auto& seq : seqs) {
        sf::Texture& tex = seq.texture;
        View* view = seq.view;

        ImGui::SetNextWindowSize((ImVec2) tex.getSize(), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(32, 32), ImVec2(FLT_MAX, FLT_MAX), CustomConstraints::AspectRatio, &tex);

        char buf[512];
        snprintf(buf, sizeof(buf), "%s###%s", seq.filenames[seq.player->frame - 1].c_str(), seq.glob.c_str());
        ImGui::Begin(buf, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

        sf::Vector2f u, v;
        view->compute(tex, u, v);
        ImGui::Image(&tex, ImGui::GetContentRegionAvail(), u, v);

        if (ImGui::IsItemHovered()) {
            if (!ImGui::IsMouseDown(2) && ImGui::GetIO().MouseWheel != 0.f) {
                view->zoom *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
            }

            ImVec2 drag = ImGui::GetMouseDragDelta(1);
            ImGui::GetIO().MouseDrawCursor = 0;
            if (drag.x || drag.y) {
                ImGui::ResetMouseDragDelta(1);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
                ImGui::GetIO().MouseDrawCursor = 1;
                view->center -= (sf::Vector2f) drag / view->zoom;
            }
            if (ImGui::IsMouseDown(2)) {
                if (ImGui::GetIO().MouseWheel != 0.f) {
                    view->smallzoomfactor *= 1 + 0.1 * ImGui::GetIO().MouseWheel;
                }
                sf::Vector2f mousePos = (sf::Vector2f) ImGui::GetMousePos() - (sf::Vector2f) ImGui::GetWindowPos();
                float texw = (float) tex.getSize().x;
                float texh = (float) tex.getSize().y;
                float cx = view->center.x;
                float cy = view->center.y;

                float rx = mousePos.x / ImGui::GetWindowSize().x;
                float ry = mousePos.y / ImGui::GetWindowSize().y;

                ImGui::BeginTooltip();

                sf::Vector2f uu, vv;
                uu.x = u.x * (1.f - rx) + v.x * rx - 1 / (2 * view->zoom*view->smallzoomfactor);
                uu.y = u.y * (1.f - ry) + v.y * ry - 1 / (2 * view->zoom*view->smallzoomfactor);
                vv.x = u.x * (1.f - rx) + v.x * rx + 1 / (2 * view->zoom*view->smallzoomfactor);
                vv.y = u.y * (1.f - ry) + v.y * ry + 1 / (2 * view->zoom*view->smallzoomfactor);

                ImGui::Image(&tex, ImVec2(128, 128*texh/texw), uu, vv);
                ImGui::Text("about (%.0f, %.0f)", (uu.x+vv.x)/2*texw, (uu.y+vv.y)/2*texh);
                ImGui::EndTooltip();
            }
        }

        ImGui::End();
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

