#include <cmath>

#include <SFML/Window/Event.hpp>
#include "imgui.h"

#include "Player.hpp"
#include "Sequence.hpp"
#include "globals.hpp"

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

    int index = std::find(gPlayers.begin(), gPlayers.end(), this) - gPlayers.begin();
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
