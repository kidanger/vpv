#pragma once

#include <string>
#include <cstdint>
#include <unordered_set>

struct Sequence;

struct Player {
private:
    std::unordered_set<struct Sequence*> sequences;

public:
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

    void onSequenceAttach(struct Sequence* s);
    void onSequenceDetach(struct Sequence* s);

    bool parseArg(const std::string& arg);
};

