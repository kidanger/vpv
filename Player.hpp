#pragma once

#include <string>

#include <SFML/System/Clock.hpp>

class Sequence;

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

    bool opened;

    Player() {
        static int id = 0;
        id++;
        ID = "Player " + std::to_string(id);

        frame = 1;
        minFrame = 1;
        maxFrame = 10000;
        currentMinFrame = 1;
        currentMaxFrame = maxFrame;
        opened = true;
    }

    void update();
    void displaySettings();
    void checkShortcuts();
    void checkBounds();
    void configureWithSequence(const Sequence& seq);
};

