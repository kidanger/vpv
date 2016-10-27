#include <cmath>
#include <glob.h>
#include <iostream>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "imgui.h"
#include "imgui-SFML.h"

int frame = 1;
int maxframe = 1;
float fps = 30.f;
bool playing = 1;
bool looping = 1;
sf::Clock frameClock;

std::vector<std::vector<std::string> > sequences;
std::vector<sf::Texture> textures;

void player();
void theme();

int main(int argc, char** argv)
{
    sf::RenderWindow window(sf::VideoMode(640, 480), "Video Viewer");
    window.setVerticalSyncEnabled(true);
    ImGui::SFML::Init(window);
    theme();

    maxframe = 10000;
    sequences.resize(argc - 1);
    textures.resize(argc - 1);
    for (int i = 0; i < argc - 1; i++) {
        glob_t res;
        glob(argv[i + 1], GLOB_TILDE, NULL, &res);
        sequences[i].resize(res.gl_pathc);
        for(unsigned int j = 0; j < res.gl_pathc; j++) {
            sequences[i][j] = res.gl_pathv[j];
        }
        globfree(&res);


        maxframe = fmin(maxframe, sequences[i].size());

        textures[i].loadFromFile(sequences[i][0]);
    }

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        ImGui::SFML::Update(deltaClock.restart());

        int oldframe = frame;

        for (int i = 0; i < sequences.size(); i++) {
            ImGui::Begin("Image " + i, 0, ImGuiWindowFlags_AlwaysAutoResize);
            float max = fmax(textures[i].getSize().x, textures[i].getSize().y);
            float w = fmax(window.getSize().x, window.getSize().y) * textures[i].getSize().x / max;
            float h = fmax(window.getSize().x, window.getSize().y) * textures[i].getSize().y / max;
            ImGui::Image(textures[i], ImVec2(w, h));
            ImGui::End();
        }

        player();

        if (frame != oldframe) {
            for (int i = 0; i < sequences.size(); i++) {
                textures[i].loadFromFile(sequences[i][frame - 1]);
            }
        }

        window.clear();
        ImGui::Render();
        window.display();
    }

    ImGui::SFML::Shutdown();
}

void player()
{
    ImGui::Begin("Player", 0, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("<")) {
        frame--;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Play", &playing))
        frameClock.restart();
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        frame++;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Looping", &looping);
    ImGui::SliderInt("Frame", &frame, 1, maxframe);
    ImGui::SliderFloat("FPS", &fps, -100.f, 100.f, "%.2f frames/s", 2);

    if (playing) {
        if (frameClock.getElapsedTime().asSeconds() > 1 / fabsf(fps)) {
            frame += fps >= 0 ? 1 : -1;
            frameClock.restart();
        }
    }

    if (frame > maxframe) {
        if (looping)
            frame = 1;
        else
            frame = maxframe;
    }
    if (frame < 0) {
        if (looping)
            frame = maxframe;
        else
            frame = 1;
    }

    for (int i = 0; i < sequences.size(); i++) {
        ImGui::Text(sequences[i][frame - 1].c_str());
    }
    ImGui::End();
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

