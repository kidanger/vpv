#pragma once

#include <set>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <queue>
#include <map>

#include "imgui.h"

#if 0
struct ImageTile {
    unsigned id;
    int x, y;
    size_t w, h, c;
    size_t scale;
    std::vector<float> pixels;
};
#endif

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
    std::vector<std::vector<std::shared_ptr<Chunk>>> chunks;
    float min, max;

    Band(size_t w, size_t h) {
        w = w / CHUNK_SIZE+1;
        h = h / CHUNK_SIZE+1;
        chunks.resize(w);
        for (int i = 0; i < w; i++) {
            chunks[i].resize(h);
        }
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
    }
};

struct Image {
    std::string ID;
    float* pixels;
    size_t w, h;
    size_t c;
    ImVec2 size;
    std::map<BandIndex, std::shared_ptr<Band>> bands;
    float min;
    float max;
    uint64_t lastUsed;
    std::shared_ptr<Histogram> histogram;

    std::set<std::string> usedBy;
    std::queue<ChunkRequest> chunkRequests;
    std::shared_ptr<ChunkProvider> chunkProvider;

    Image(float* pixels, size_t w, size_t h, size_t c);
    Image(size_t w, size_t h, size_t c);
    ~Image();

    std::shared_ptr<Band> getBand(size_t bandidx) {
        auto it = bands.find(bandidx);
        if (it != bands.end()) {
            std::shared_ptr<Band> b = it->second;
            return b;
        }
        return nullptr;
    }

    void getPixelValueAt(size_t x, size_t y, float* values, size_t d) const;
    std::array<bool,3> getPixelValueAtBands(size_t x, size_t y, BandIndices bands, float* values) const;

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

