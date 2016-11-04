#pragma once

#include <string>
#include <vector>
#include <map>

#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

namespace sf {
  class Texture;
}

class View;
class Player;

struct Sequence {
    std::string ID;
    std::string glob;
    std::string glob_;
    std::vector<std::string> filenames;
    bool valid;
    bool visible;

    int loadedFrame;
    ImRect loadedRect;

    std::map<int, sf::Image> pixelCache;

    sf::Texture texture;
    View* view;
    Player* player;

    Sequence();

    void loadFilenames();

    void loadFrame(int frame);
    void loadTextureIfNeeded();

};
