#include <string>

#include "imgui.h"

#include "Shader.hpp"

Shader::Shader()
{
    static int id = 0;
    id++;
    ID = "Shader " + std::to_string(id);
}

bool Shader::compile()
{
    return shader.loadFromMemory(codeVertex, std::string(codeTonemap) + codeFragment);
}

