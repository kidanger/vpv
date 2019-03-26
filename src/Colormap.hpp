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
    void getRange(std::array<float,3>& min, std::array<float,3>& max) const;
    std::array<float, 3> getScale() const;
    std::array<float, 3> getBias() const;

    void autoCenterAndRadius(float min, float max);

    void nextShader();
    void previousShader();
    std::string getShaderName() const;
    bool setShader(const std::string& name);
};

