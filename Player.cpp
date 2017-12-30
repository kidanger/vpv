#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

#include <SFML/Window/Event.hpp>
#include "imgui.h"

#include "Player.hpp"
#include "Sequence.hpp"
#include "globals.hpp"

Player::Player() {
    static int id = 0;
    id++;
    ID = "Player " + std::to_string(id);

    frame = 1;
    minFrame = 1;
    maxFrame = std::numeric_limits<int>::max();
    currentMinFrame = 1;
    currentMaxFrame = maxFrame;
    opened = true;
}

void Player::update()
{
    frameAccumulator += frameClock.restart();

    if (playing) {
        while (frameAccumulator.asSeconds() > 1 / fabsf(fps)) {
            frame += fps >= 0 ? 1 : -1;
            frameAccumulator -= sf::seconds(1 / fabsf(fps));
            checkBounds();
        }
    } else {
        frameAccumulator = sf::seconds(0);
    }

    int index = std::find(gPlayers.begin(), gPlayers.end(), this) - gPlayers.begin();
    bool isKeyFocused = !ImGui::GetIO().WantCaptureKeyboard && index <= 9 &&
        ImGui::IsKeyPressed(sf::Keyboard::Num1 + index) && ImGui::IsKeyDown(sf::Keyboard::LAlt);

    if (isKeyFocused)
        opened = true;

    if (!opened) {
        return;
    }

    if (!ImGui::Begin(ID.c_str(), &opened, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    if (isKeyFocused) {
        ImGui::SetWindowFocus();
    }

    if (ImGui::IsWindowFocused()) {
        checkShortcuts();
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
    ImGui::SameLine(); ImGui::ShowHelpMarker("Previous frame (left)");
    ImGui::SameLine();
    ImGui::Checkbox("Play", &playing);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Toggle playback (p)");
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        frame++;
        playing = 0;
    }
    ImGui::SameLine(); ImGui::ShowHelpMarker("Next frame (right)");
    ImGui::SameLine();
    ImGui::Checkbox("Looping", &looping);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Loops when at the end of the sequence");
    if (ImGui::SliderInt("Frame", &frame, currentMinFrame, currentMaxFrame)) {
        playing = 0;
    }
    ImGui::SliderFloat("FPS", &fps, -100.f, 100.f, "%.2f frames/s");
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the Frame Per Second rate");
    ImGui::DragIntRange2("Bounds", &currentMinFrame, &currentMaxFrame, 1.f, minFrame, maxFrame);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the bounds of the playback");
}

void Player::checkShortcuts()
{
    if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::P, false)) {
        playing = !playing;
    }
    if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::Left)) {
        frame--;
        checkBounds();
    }
    if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::Right)) {
        frame++;
        checkBounds();
    }
    if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::F8)) {
        fps -= 1;
    }
    if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(sf::Keyboard::F9)) {
        fps += 1;
    }
}

void Player::checkBounds()
{
    currentMaxFrame = std::min(currentMaxFrame, maxFrame);
    currentMinFrame = std::max(currentMinFrame, minFrame);
    currentMaxFrame = std::max(currentMaxFrame, currentMinFrame);
    currentMinFrame = std::min(currentMinFrame, currentMaxFrame);

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

void Player::reconfigureBounds()
{
    minFrame = 1;
    maxFrame = std::numeric_limits<int>::max();
    currentMinFrame = minFrame;
    currentMaxFrame = maxFrame;

    for (auto seq : gSequences) {
        if (seq->player == this)
            maxFrame = fmin(maxFrame, seq->filenames.size());
    }

    checkBounds();
}
