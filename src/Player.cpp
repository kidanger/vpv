#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

#include "imgui.h"

#include "Player.hpp"
#include "Sequence.hpp"
#include "ImageCollection.hpp"
#include "globals.hpp"
#include "events.hpp"

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

    fps = gDefaultFramerate;
    frameClock = 0;
    frameAccumulator = 0.;
}

void Player::update()
{
    frameAccumulator += letTimeFlow(&frameClock);

    if (playing) {
        while (frameAccumulator > 1000. / std::abs(fps)) {
            frame += fps >= 0 ? 1 : -1;
            frameAccumulator -= 1000. / std::abs(fps);
            checkBounds();
        }
    } else {
        frameAccumulator = 0.;
    }

    int index = std::find(gPlayers.begin(), gPlayers.end(), this) - gPlayers.begin();
    char d[2] = {static_cast<char>('1' + index), 0};
    bool isKeyFocused = index <= 9 && isKeyPressed(d) && isKeyDown("alt");

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
    if (isKeyPressed("p", false)) {
        playing = !playing;
    }
    if (isKeyPressed("left")) {
        frame--;
        checkBounds();
    }
    if (isKeyPressed("right")) {
        frame++;
        checkBounds();
    }
    if (isKeyPressed("F8")) {
        fps -= 1;
    }
    if (isKeyPressed("F9")) {
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
        if (seq->player == this && seq->collection)
            maxFrame = fmin(maxFrame, seq->collection->getLength());
    }

    checkBounds();
}

