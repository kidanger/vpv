#pragma once

#include <cmath>
#include <set>
#include <memory>
#include <algorithm>
#include <limits>
#include <string>
#include <array>
#include <vector>
#include <queue>
#include <map>

#include "imgui.h"

#include "globals.hpp"

typedef size_t BandIndex;
typedef std::array<BandIndex,3> BandIndices;
#define BANDS_DEFAULT (BandIndices{0,1,2})

class Histogram;

#define CHUNK_SIZE ((size_t)1024)

struct ChunkRequest {
    BandIndex bandidx;
    size_t cx, cy;
};

class ChunkProvider {
public:
    virtual ~ChunkProvider() {}
    virtual void process(ChunkRequest cr, struct Image* image) = 0;
};

struct Chunk {
private:
    bool validated = false;
    std::map<float, float> quantiles;

public:
    std::array<float, CHUNK_SIZE*CHUNK_SIZE> pixels;
    int w, h;
    float min, max;

    float getQuantile(float q) {
        return quantiles[q];
    }

    void validate() {
        // TODO: this is costly, subsample?
        std::vector<float> all(pixels.begin(), pixels.begin()+w*h);
        all.erase(std::remove_if(all.begin(), all.end(),
                                 [](float x){return !std::isfinite(x);}),
                  all.end());
        std::sort(all.begin(), all.end());
        for (auto q : gSaturations) {
            float v;
            quantiles[q] = all[q*all.size()];
            quantiles[1-q] = all[(1-q)*all.size()];
        }
        min = all.front();
        max = all.back();
        validated = true;
    }

    bool areStatsValid() {
        return validated;
    }
};

struct Band {
private:
    size_t w, h;

public:
    std::vector<std::vector<std::shared_ptr<Chunk>>> chunks;
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::lowest();

    Band(size_t w, size_t h) : w(w), h(h) {
        clear();
    }

    std::shared_ptr<Chunk> getChunk(size_t x, size_t y) const {
        x = x / CHUNK_SIZE;
        y = y / CHUNK_SIZE;
        return chunks[x][y];
    }

    void setChunk(size_t x, size_t y, std::shared_ptr<Chunk> chunk) {
        x = x / CHUNK_SIZE;
        y = y / CHUNK_SIZE;
        if (chunks[x][y]) {
            printf("something is wrong! setChunk twice\n");
        }
        chunks[x][y] = chunk;
        validateChunk(x * CHUNK_SIZE, y * CHUNK_SIZE);
    }

    void validateChunk(size_t x, size_t y) {
        x = x / CHUNK_SIZE;
        y = y / CHUNK_SIZE;
        std::shared_ptr<Chunk>& ck = chunks[x][y];
        if (!ck) {
            printf("something is wrong! validateChunk\n");
        }
        if (ck->areStatsValid()) {
            printf("something is wrong! validateChunk 2\n");
        }
        ck->validate();
        min = std::min(min, ck->min);
        max = std::max(max, ck->max);
    }

    void clear() {
        min = std::numeric_limits<float>::max();
        max = std::numeric_limits<float>::lowest();
        chunks.clear();
        size_t cw = w / CHUNK_SIZE + 1;
        size_t ch = h / CHUNK_SIZE + 1;
        chunks.resize(cw);
        for (int i = 0; i < cw; i++) {
            chunks[i].resize(ch);
        }
    }
};

struct Image {
    std::string ID;
    size_t w, h;
    size_t c;
    ImVec2 size;
    std::map<BandIndex, std::shared_ptr<Band>> bands;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;
    std::queue<ChunkRequest> chunkRequests;
    std::shared_ptr<ChunkProvider> chunkProvider;

    Image(size_t w, size_t h, size_t c);
    ~Image();

    std::shared_ptr<Band> getBand(BandIndex bandidx) const {
        auto it = bands.find(bandidx);
        if (it != bands.end()) {
            std::shared_ptr<Band> b = it->second;
            return b;
        }
        return nullptr;
    }

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool,3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

    float getBandMin(BandIndex b) const;
    float getBandMax(BandIndex b) const;
    float getBandsMin(BandIndices bands) const;
    float getBandsMax(BandIndices bands) const;

    bool hasAtLeastOneValidChunk(BandIndices bands) const {
        for (int i = 0; i < 3; i++) {
            std::shared_ptr<Band> band = getBand(bands[i]);
            if (band) {
                for (auto cc : band->chunks) {
                    for (auto c : cc) {
                        if (c && c->areStatsValid())
                            return true;
                    }
                }
            }
        }
        return false;
    }

    void requestChunkAtBand(size_t b, size_t x, size_t y) {
        std::shared_ptr<Band> band = getBand(b);
        if (!band)
            return;
        x = x / CHUNK_SIZE;
        y = y / CHUNK_SIZE;
        ChunkRequest cr {
            b, x, y
        };
        chunkRequests.push(cr);
    }
};

std::shared_ptr<Image> create_image_from_filename(const std::string& filename);

