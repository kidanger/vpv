#pragma once

#include <string>
#include <map>

#include <SFML/Graphics/Shader.hpp>

extern std::map<std::string, sf::Shader> gShaders;

void loadShaders();

