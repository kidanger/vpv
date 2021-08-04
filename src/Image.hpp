#pragma once

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <atomic>

#include <imgui.h>

#if 0
struct ImageTile {
    unsigned id;
    int x, y;
    size_t w, h, c;
    size_t scale;
    std::vector<float> pixels;
};
#endif

using BandIndices = std::array<size_t, 3>;
#define BANDS_DEFAULT (BandIndices { 0, 1, 2 })

class Histogram;

struct Image : public std::enable_shared_from_this<Image> {
    std::string ID;
    float* pixels;
    size_t w, h, c;
    ImVec2 size;
    float min;
    float max;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    Image(float* pixels, size_t w, size_t h, size_t c);
    ~Image();

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool, 3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

    bool hasMinMaxStats()
    {
        return _statsState >= MINMAX;
    }
    bool hasChunkMinMaxStats()
    {
        return _statsState >= CHUNK_MINMAX;
    }
    bool hasQuantilesStats()
    {
        return _statsState >= QUANTILES;
    }

    void computeStats();
    void computeStatsLater();

    int getStatChunkSize() const
    {
        return STAT_CHUNK_SIZE;
    }
    float getChunkMin(size_t b, size_t cx, size_t cy) const
    {
        return _statsChunks[b][cx][cy].min;
    }
    float getChunkMax(size_t b, size_t cx, size_t cy) const
    {
        return _statsChunks[b][cx][cy].max;
    }
    float getChunkQuantile(size_t b, size_t cx, size_t cy, float q) const
    {
        const StatChunk& cs = _statsChunks[b][cx][cy];
        auto it = cs.quantiles.find(q);
        if (it != cs.quantiles.end()) {
            return it->second;
        }
        return 0.f;
    }

private:
    enum LoadingState {
        NONE,
        STARTED,
        MINMAX,
        CHUNK_MINMAX,
        QUANTILES,
        DONE = QUANTILES,
    };
    std::atomic<LoadingState> _statsState;

    const int STAT_CHUNK_SIZE = 128;
    struct StatChunk {
        std::map<float, float> quantiles;
        float min, max;
    };

    std::vector<std::vector<std::vector<StatChunk>>> _statsChunks; // band, cx, cy
};

std::shared_ptr<Image> popImageFromStatQueue();
