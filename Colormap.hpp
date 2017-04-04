#pragma once

#include <string>

struct Shader;

struct Colormap
{
    std::string ID;
    float scale;
    float bias;
    Shader* shader;

    Colormap();

    void displaySettings();
    void getRange(float& min, float& max) const;
    void print() const;
    void nextShader();
    void previousShader();
    std::string getShaderName() const;
};

