#pragma once

#include <string>

struct Shader;

struct Colormap
{
    std::string ID;
    float center;
    float radius;
    Shader* shader;

    Colormap();

    void displaySettings();
    void getRange(float& min, float& max) const;
    float getScale() const;
    float getBias() const;

    void autoCenterAndRadius(float min, float max);

    void print() const;
    void nextShader();
    void previousShader();
    std::string getShaderName() const;
};

