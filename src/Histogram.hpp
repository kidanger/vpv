#pragma once

#include <vector>
#include <memory>
#include <mutex>

#include "Progressable.hpp"

struct Image;

class Histogram : public Progressable {
private:
    bool loaded;
    mutable std::recursive_mutex lock;
public:
    enum Mode {
        SMOOTH,
        EXACT,
    } mode;
    float min, max;
    std::vector<std::vector<long>> values;
    std::weak_ptr<Image> image;
    size_t curh;
    const int nbins;

public:
    Histogram() : loaded(true), image(std::weak_ptr<Image>()), curh(0), nbins(256) {}

    void request(std::shared_ptr<Image> image, float min, float max, Mode mode);

    float getProgressPercentage() const;

    bool isLoaded() const {
        return loaded;
    }

    void progress();

    void draw(const std::array<float,3>& highlightmin, const std::array<float,3>& highlightmax,
              const float* highlights);

};


