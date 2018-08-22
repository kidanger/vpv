#pragma once

#include <string>
#include <array>

struct Shader;

struct Colormap
{
    std::string ID;
    std::array<float,3> center;
    float radius;
    Shader* shader;
    bool initialized;

    Colormap();

    void displaySettings();
    void getRange(float& min, float& max, int n) const;
    std::array<float, 3> getScale() const;
    std::array<float, 3> getBias() const;

    void autoCenterAndRadius(float min, float max);

    void nextShader();
    void previousShader();
    std::string getShaderName() const;
};

