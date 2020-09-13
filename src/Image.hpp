#pragma once

#include <cmath>
#include <set>
#include <memory>
#include <limits>
#include <string>
#include <array>
#include <vector>
#include <queue>
#include <map>

#include "imgui.h"

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
    std::array<float, CHUNK_SIZE*CHUNK_SIZE> pixels;
    int w, h;
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
        chunks[x][y] = chunk;
        for (int i = 0; i < chunk->w * chunk->h; i++) {
            float v = chunk->pixels[i];
            if (std::isfinite(v)) {
                min = std::min(min, v);
                max = std::max(max, v);
            }
        }
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

    bool hasAtLeastOneChunk(BandIndices bands) const {
        for (int i = 0; i < 3; i++) {
            std::shared_ptr<Band> band = getBand(bands[i]);
            if (band) {
                for (auto cc : band->chunks) {
                    for (auto c : cc) {
                        if (c)
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

