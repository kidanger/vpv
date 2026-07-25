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
    // bumped by every request() that actually restarts the computation; used by
    // progress() to detect that the work it just did is stale
    uint64_t requestGeneration;
    // generation of the Image stats the current bins were computed with
    uint64_t statsGeneration;

public:
    enum class Mode {
        SMOOTH,
        EXACT,
    } mode;
    float min, max;
    std::vector<std::vector<long>> values;
    std::weak_ptr<Image> image;
    size_t curh;
    const int nbins;
    ImRect region;

public:
    Histogram()
        : loaded(true)
        , requestGeneration(0)
        , statsGeneration(0)
        , mode(Mode::EXACT)
        , min(0.f)
        , max(0.f)
        , image(std::weak_ptr<Image>())
        , curh(0)
        , nbins(256)
        , region()
    {
    }

    void request(std::shared_ptr<Image> image, Mode mode, ImRect region = ImRect(0, 0, 0, 0));

    float getProgressPercentage() const override;

    bool isLoaded() const override
    {
        std::lock_guard<std::recursive_mutex> _lock(lock);
        return loaded;
    }

    void progress() override;

    void draw(const Colormap& colormap, const float* highlights);
};
