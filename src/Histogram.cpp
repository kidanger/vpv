#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "imgui_custom.hpp"

#include "Image.hpp"
#include "Histogram.hpp"

void Histogram::request(std::shared_ptr<Image> image, float min, float max, Mode mode) {
    std::lock_guard<std::mutex> _lock(lock);
    std::shared_ptr<Image> img = this->image.lock();
    if (image == img && min == this->min && max == this->max && mode == this->mode)
        return;
    loaded = false;
    this->mode = mode;
    this->min = min;
    this->max = max;
    this->image = image;
    curh = 0;

    values.clear();
    values.resize(image->c);

    for (size_t d = 0; d < image->c; d++) {
        auto& histogram = values[d];
        histogram.clear();
        histogram.resize(nbins);
    }
}

float Histogram::getProgressPercentage() const {
    std::shared_ptr<Image> image = this->image.lock();
    if (loaded) return 1.f;
    if (!image) return 0.f;
    return (float) curh / image->h;
}

void Histogram::progress()
{
    std::vector<std::vector<long>> valuescopy;
    size_t oldh;
    {
        std::lock_guard<std::mutex> _lock(lock);
        valuescopy = values;
        oldh = curh;
    }

    std::shared_ptr<Image> image = this->image.lock();
    if (!image) return;
    for (size_t d = 0; d < image->c; d++) {
        auto& histogram = valuescopy[d];
        // nbins-1 because we want the last bin to end at 'max' and not start at 'max'
        float f = (nbins-1) / (max - min);
        for (size_t i = 0; i < image->w; i++) {
            int bin = (image->pixels[(curh*image->h + i)*image->c+d] - min) * f;
            if (bin >= 0 && bin < nbins) {
                histogram[bin]++;
            }
        }
    }

    {
        std::lock_guard<std::mutex> _lock(lock);
        if (oldh != curh) {
            // someone called request()
            return;
        }
        curh++;

        if (curh == image->h) {
            loaded = true;
        }

        values = valuescopy;
    }
}

void Histogram::draw() const
{
    std::lock_guard<std::mutex> _lock(lock);
    const size_t c = values.size();

    const void* vals[4];
    for (size_t d = 0; d < c; d++) {
        vals[d] = this->values[d].data();
    }

    const char* names[] = {"r", "g", "b", ""};
    ImColor colors[] = {
        ImColor(255, 0, 0), ImColor(0, 255, 0), ImColor(0, 0, 255), ImColor(100, 100, 100)
    };
    if (c == 1) {
        colors[0] = ImColor(255, 255, 255);
        names[0] = "";
    }
    auto getter = [](const void *data, int idx) {
        const long* hist = (const long*) data;
        return (float)hist[idx];
    };

    ImGui::Separator();
    ImGui::PlotMultiHistograms("", c, names, colors, getter, vals,
                               nbins, FLT_MIN, FLT_MAX, ImVec2(nbins, 80));
    if (!isLoaded()) {
        const int barw = 100;
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        const ImU32 bg = ImColor(100,100,100);
        ImGui::BufferingBar("##bar", getProgressPercentage(),
                            ImVec2(barw, 6), bg, col);
    }
}

