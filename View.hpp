#pragma once

#include <string>

#include "imgui.h"

namespace sf {
  class Texture;
}

struct View {
    std::string ID;

    float zoom;
    float smallzoomfactor;
    ImVec2 center;

    View();

    void compute(const sf::Texture& tex, sf::Vector2f& u, sf::Vector2f& v) const;

    void displaySettings();

};

