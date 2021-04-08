#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>
#include <memory>
#include <utility>

#include <doctest.h>
#include <imgui.h>

#include "Player.hpp"
#include "Sequence.hpp"
#include "ImageCollection.hpp"
#include "globals.hpp"
#include "events.hpp"
#include "strutils.hpp"

Player::Player() {
    static int id = 0;
    id++;
    ID = "Player " + std::to_string(id);

    frame = 1;
    minFrame = 1;
    maxFrame = std::numeric_limits<int>::max();
    currentMinFrame = 1;
    currentMaxFrame = maxFrame;
    opened = false;

    fps = gDefaultFramerate;
    frameClock = 0;
    frameAccumulator = 0.;
}

bool Player::operator==(const Player& other) {
    return other.ID == ID;
}

void Player::update()
{
    frameAccumulator += letTimeFlow(&frameClock);

    if (!bouncy) {
        direction = 1;
    }

    if (playing) {
        while (frameAccumulator > 1000. / std::abs(fps)) {
            int d = (fps >= 0 ? 1 : -1) * direction;
            frame += d;
            frameAccumulator -= 1000. / std::abs(fps);
            if (bouncy) {
                if (frame < currentMinFrame) {
                    frame = currentMinFrame + 1;
                    direction *= -1;
                }
                if (frame > currentMaxFrame) {
                    frame = currentMaxFrame - 1;
                    direction *= -1;
                }
            }
            checkBounds();
        }
    } else {
        frameAccumulator = 0.;
    }

    int index = std::find(gPlayers.begin(), gPlayers.end(), shared_from_this()) - gPlayers.begin();
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
    ImGui::BringFront();

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
        playing = false;
    }
    ImGui::SameLine(); ImGui::ShowHelpMarker("Previous frame (left)");
    ImGui::SameLine();
    ImGui::Checkbox("Play", &playing);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Toggle playback (p)");
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        frame++;
        playing = false;
    }
    ImGui::SameLine(); ImGui::ShowHelpMarker("Next frame (right)");
    ImGui::Checkbox("Looping", &looping);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Loops when at the end of the sequence");
    ImGui::SameLine(); ImGui::Checkbox("Bouncy", &bouncy);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Bounce back and forth instead of circular playback");
    if (ImGui::SliderInt("Frame", &frame, currentMinFrame, currentMaxFrame)) {
        playing = false;
    }
    ImGui::SliderFloat("FPS", &fps, -100.f, 100.f, "%.2f frames/s");
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the Frame Per Second rate");
    ImGui::DragIntRange2("Bounds", &currentMinFrame, &currentMaxFrame, 1.f, minFrame, maxFrame);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Change the bounds of the playback");
}

void Player::checkShortcuts()
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || isKeyDown("control") || isKeyDown("alt") || isKeyDown("shift")) {
        return;
    }
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
        if (looping) {
            frame = currentMinFrame;
        } else {
            frame = currentMaxFrame;
        }
    }
    if (frame < currentMinFrame) {
        if (looping) {
            frame = currentMaxFrame;
        } else {
            frame = currentMinFrame;
        }
    }
}

void Player::reconfigureBounds()
{
    minFrame = 1;
    maxFrame = 0;
    currentMinFrame = minFrame;
    currentMaxFrame = std::numeric_limits<int>::max();

    std::vector<std::weak_ptr<Sequence>> to_remove;
    for (const auto& ptr : sequences) {
        if (const auto& seq = ptr.lock()) {
            if (seq->collection) {
                int len = seq->collection->getLength();
                maxFrame = std::max(maxFrame, len);
            }
        } else {
            to_remove.push_back(ptr);
        }
    }
    for (const auto& ptr : to_remove) {
        sequences.erase(ptr);
    }

    checkBounds();
}

void Player::onSequenceAttach(std::weak_ptr<Sequence> s)
{
    sequences.insert(std::move(s));
    reconfigureBounds();
}

void Player::onSequenceDetach(std::weak_ptr<Sequence> s)
{
    sequences.erase(std::move(s));
    reconfigureBounds();
}

bool Player::parseArg(const std::string& arg)
{
    if (startswith(arg, "p:fps:")) {
        float new_fps = gDefaultFramerate;
        if (sscanf(arg.c_str(), "p:fps:%f", &new_fps) == 1) {
            fps = new_fps;
            return true;
        }
    } else if (arg == "p:play") {
        playing = true;
        return true;
    } else if (startswith(arg, "p:looping")) {
        int new_looping;
        if (sscanf(arg.c_str(), "p:looping:%d", &new_looping) == 1) {
            looping = new_looping;
            return true;
        }
    } else if (startswith(arg, "p:bouncing")) {
        int new_bouncing;
        if (sscanf(arg.c_str(), "p:bouncing:%d", &new_bouncing) == 1) {
            bouncy = new_bouncing;
            return true;
        }
    } else if (startswith(arg, "p:direction")) {
        int new_direction;
        if (sscanf(arg.c_str(), "p:direction:%d", &new_direction) == 1) {
            if (new_direction == -1 || new_direction == 1) {
                direction = new_direction;
                return true;
            }
        }
    } else if (startswith(arg, "p:frame:")) {
        int new_frame;
        if (sscanf(arg.c_str(), "p:frame:%d", &new_frame) == 1) {
            if (new_frame >= 1) {
                frame = new_frame;
                return true;
            }
        }
    }
    return false;
}

TEST_CASE("Player::parseArg") {
    Player p;

    SUBCASE("p:fps:") {
        CHECK(p.fps == gDefaultFramerate);
        CHECK(p.parseArg("p:fps:120.5"));
        CHECK(p.fps == doctest::Approx(120.5f));
        CHECK(p.parseArg("p:fps:-10"));
        CHECK(p.fps == doctest::Approx(-10.f));
    }

    SUBCASE("p:play") {
        CHECK(!p.playing);
        CHECK(p.parseArg("p:play"));
        CHECK(p.playing);
    }

    SUBCASE("p:looping") {
        CHECK(p.looping);
        CHECK(p.parseArg("p:looping:0"));
        CHECK(!p.looping);
        CHECK(p.parseArg("p:looping:1"));
        CHECK(p.looping);
    }

    SUBCASE("p:bouncing") {
        CHECK(!p.bouncy);
        CHECK(p.parseArg("p:bouncing:1"));
        CHECK(p.bouncy);
        CHECK(p.parseArg("p:bouncing:0"));
        CHECK(!p.bouncy);
    }

    SUBCASE("p:direction") {
        CHECK(p.direction == 1);
        CHECK(p.parseArg("p:direction:-1"));
        CHECK(p.direction == -1);
        CHECK(p.parseArg("p:direction:1"));
        CHECK(p.direction == 1);
        SUBCASE("p:directionn invalid") {
            CHECK(!p.parseArg("p:direction:0"));
            CHECK(p.direction == 1);
            CHECK(!p.parseArg("p:direction:2"));
            CHECK(p.direction == 1);
        }
    }

    SUBCASE("p:frame") {
        CHECK(p.frame == 1);
        CHECK(p.parseArg("p:frame:10"));
        CHECK(p.frame == 10);
        SUBCASE("p:frame invalid") {
            CHECK(!p.parseArg("p:frame:-10"));
            CHECK(p.frame == 10);
        }
    }
}

