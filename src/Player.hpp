#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <set>

struct Sequence;

struct Player: std::enable_shared_from_this<Player> {
private:
    std::set<std::weak_ptr<struct Sequence>, std::owner_less<std::weak_ptr<struct Sequence>>> sequences;

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

    bool operator==(const Player& other);

    void update();
    void displaySettings();
    void checkShortcuts();
    void checkBounds();
    void reconfigureBounds();

    void onSequenceAttach(std::weak_ptr<struct Sequence> s);
    void onSequenceDetach(std::weak_ptr<struct Sequence> s);

    bool parseArg(const std::string& arg);
};

