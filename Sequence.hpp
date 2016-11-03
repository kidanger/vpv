#pragma once

#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <vector>

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

    sf::Texture texture;
    View* view;
    Player* player;

    Sequence();

    void loadFilenames();

    void loadTextureIfNeeded();

};
