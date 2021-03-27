#pragma once

#include <string>
#include <array>

#include "Image.hpp"  // for bands

struct Shader;

struct Colormap
{
    std::string ID;
    std::array<float,3> center;
    float radius;
    Shader* shader;
    bool initialized;
    int currentSat;
    BandIndices bands;

    Colormap();

    void displaySettings();
    void getRange(float& min, float& max, int n) const;
    void getRange(std::array<float,3>& min, std::array<float,3>& max) const;
    std::array<float, 3> getScale() const;
    std::array<float, 3> getBias() const;

    void autoCenterAndRadius(float min, float max);

    void nextShader();
    void previousShader();
    const std::string& getShaderName() const;
    bool setShader(const std::string& name);

    bool bandsAreStandard() const { return bands[0] == 0 && bands[1] == 1 && bands[2] == 2; }
};

