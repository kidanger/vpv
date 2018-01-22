#pragma once

#include <string>

#include <SFML/Graphics/Shader.hpp>

#define SHADER_CODE_SIZE (1<<14)

struct Shader {
    std::string ID;
    std::string name;
    sf::Shader shader;

    char codeVertex[SHADER_CODE_SIZE];
    char codeFragment[SHADER_CODE_SIZE];

    Shader();

    bool compile();
};

