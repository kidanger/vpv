#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "Progressable.hpp"

struct Image;
struct Colormap;

class Histogram : public Progressable {
private:
    bool loaded;
    mutable std::recursive_mutex lock;

public:
    float min, max;
    std::vector<std::vector<long>> values;
    std::weak_ptr<Image> image;
    size_t curh;
    const int nbins;
    ImRect region;

public:
    Histogram()
        : loaded(true)
        , image(std::weak_ptr<Image>())
        , curh(0)
        , nbins(256)
        , region()
    {
    }

    void request(std::shared_ptr<Image> image, ImRect region = ImRect(0, 0, 0, 0));

    float getProgressPercentage() const override;

    bool isLoaded() const override
    {
        return loaded;
    }

    void progress() override;

    void draw(const Colormap& colormap, const float* highlights);
};
