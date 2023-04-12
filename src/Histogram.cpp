#include <cmath>
#include <imgui.h>
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include "imgui_custom.hpp"

#include "Colormap.hpp"
#include "Histogram.hpp"
#include "Image.hpp"
#include "globals.hpp"

void Histogram::request(std::shared_ptr<Image> image, ImRect region)
{
    std::lock_guard<std::recursive_mutex> _lock(lock);
    std::shared_ptr<Image> img = this->image.lock();
    float min = image->min;
    float max = image->max;
    if (region.Min.x == 0 && region.Min.y == 0 && region.Max.x == 0 && region.Max.y == 0) {
        region.Max.x = image->w;
        region.Max.y = image->h;
    }
    if (image == img && min == this->min && max == this->max && region == this->region)
        return;
    loaded = false;
    this->min = min;
    this->max = max;
    this->image = image;
    this->region = region;
    curh = 0;

    values.clear();
    values.resize(image->c);

    for (size_t d = 0; d < image->c; d++) {
        auto& histogram = values[d];
        histogram.clear();
        histogram.resize(nbins);
    }
}

float Histogram::getProgressPercentage() const
{
    std::shared_ptr<Image> image = this->image.lock();
    if (loaded)
        return 1.f;
    if (!image)
        return 0.f;
    return (float)curh / region.GetHeight();
}

void Histogram::progress()
{
    std::vector<std::vector<long>> valuescopy;
    size_t oldh;
    {
        std::lock_guard<std::recursive_mutex> _lock(lock);
        valuescopy = values;
        oldh = curh;
    }

    std::shared_ptr<Image> image = this->image.lock();
    if (!image)
        return;

    size_t minh = region.Min.y;
    size_t minx = region.Min.x;
    size_t maxx = region.Max.x;
    for (size_t d = 0; d < image->c; d++) {
        auto& histogram = valuescopy[d];
        // nbins-1 because we want the last bin to end at 'max' and not start at 'max'
        float f = (nbins - 1) / (max - min);
        for (size_t i = minx; i < maxx; i++) {
            // TODO: sometimes it crashes here
            int bin = (image->pixels[((minh + curh) * image->w + i) * image->c + d] - min) * f;
            if (bin >= 0 && bin < nbins) {
                histogram[bin]++;
            }
        }
    }

    {
        std::lock_guard<std::recursive_mutex> _lock(lock);
        if (oldh != curh) {
            // someone called request()
            return;
        }
        curh++;

        if (curh == region.GetHeight()) {
            loaded = true;
        }

        values = valuescopy;
    }
}

void Histogram::draw(const Colormap& colormap, const float* highlights)
{
    std::lock_guard<std::recursive_mutex> _lock(lock);
    const size_t c = std::min((size_t)3, values.size());

    std::array<float, 3> highlightmin, highlightmax;
    colormap.getRange(highlightmin, highlightmax);

    BandIndices bands = colormap.bands;
    std::array<bool, 3> bandvalids;
    const void* vals[3] = { nullptr };
    for (size_t d = 0; d < 3; d++) {
        bandvalids[d] = bands[d] < values.size();
        if (bandvalids[d]) {
            vals[d] = this->values[bands[d]].data();
        }
    }

    const char* names[] = { "r", "g", "b" };
    ImColor colors[] = {
        ImColor(255, 0, 0), ImColor(0, 255, 0), ImColor(0, 0, 255)
    };
    if (values.size() == 1 && bandvalids[0] && !bandvalids[1] && !bandvalids[2]) {
        colors[0] = ImColor(255, 255, 255);
        names[0] = "";
    }
    auto getter = [](const void* data, int idx) {
        if (!data)
            return 0.f;
        const long* hist = (const long*)data;
        return (float)hist[idx];
    };

    int boundsmin[3];
    int boundsmax[3];
    int bhighlights[3];
    float f = (nbins - 1) / (max - min);
    for (size_t d = 0; d < 3; d++) {
        boundsmin[d] = std::floor((highlightmin[d] - min) * f);
        boundsmax[d] = std::ceil((highlightmax[d] - min) * f);
        if (highlights)
            bhighlights[d] = std::floor((highlights[d] - min) * f);
    }

    ImGui::Separator();
    ImGui::PlotMultiHistograms("", 3, names, colors, getter, vals,
        nbins, FLT_MIN, curh ? FLT_MAX : 1.f, ImVec2(nbins, 80),
        boundsmin, boundsmax, highlights ? bhighlights : nullptr);

    if (!isLoaded()) {
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        const ImU32 bg = ImColor(100, 100, 100);
        ImGui::BufferingBar("##bar", getProgressPercentage(),
            ImVec2(ImGui::GetWindowWidth() - 10, 6), bg, col);
    }
}
