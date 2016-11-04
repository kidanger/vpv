#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>

#include <string>
#include <vector>
#include <map>

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

    std::map<int, sf::Image> pixelCache;

    sf::Texture texture;
    View* view;
    Player* player;

    Sequence();

    void loadFilenames();

    void loadFrame(int frame);
    void loadTextureIfNeeded();

};
