#pragma once

#include <string>
#include <cstdint>

struct Sequence;

struct Player {
    std::string ID;

    int frame;
    int currentMinFrame;
    int currentMaxFrame;
    int minFrame;
    int maxFrame;

    float fps;
    bool playing = false;
    bool looping = true;
    bool bouncy = false;
    int direction = 1;

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

