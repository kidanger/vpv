#pragma once

#include <string>
#include <cstdint>

#include <SFML/System/Clock.hpp>

class Sequence;

struct Player {
    std::string ID;

    int frame;
    int currentMinFrame;
    int currentMaxFrame;
    int minFrame;
    int maxFrame;

    float fps;
    bool playing = 0;
    bool looping = 1;

    uint64_t frameClock;
    double frameAccumulator;

    bool opened;

    Player();

    void update();
    void displaySettings();
    void checkShortcuts();
    void checkBounds();
    void reconfigureBounds();
};

