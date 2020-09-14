#include <iostream>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <algorithm>

extern "C" {
#include "iio.h"
}

#include "Image.hpp"
#include "Histogram.hpp"

Image::Image(size_t w, size_t h, size_t c)
    : w(w), h(h), c(c), lastUsed(0), histogram(std::make_shared<Histogram>())
{
    static int id = 0;
    id++;
    ID = "Image " + std::to_string(id);

    for (int i = 0; i < c; i++) {
        bands[i] = std::make_shared<Band>(w, h);
    }

    size = ImVec2(w, h);
}

Image::~Image()
{
}

void Image::getPixelValueAt(size_t x, size_t y, float* values, size_t d) const
{
    assert(0);
}

std::array<bool,3> Image::getPixelValueAtBands(size_t x, size_t y, BandIndices bandsidx, float* values) const
{
    std::array<bool,3> valids {false,false,false};
    if (x >= w || y >= h)
        return valids;

    for (size_t i = 0; i < 3; i++) {
        std::shared_ptr<Band> band = getBand(bandsidx[i]);
        if (!band) continue;
        std::shared_ptr<Chunk> ck = band->getChunk(x, y);
        if (!ck) continue;
        values[i] = ck->pixels[(y%ck->h)*ck->w+x%ck->w];
        valids[i] = true;
    }

    return valids;
}

float Image::getBandMin(BandIndex b) const
{
    std::shared_ptr<Band> band = getBand(b);
    return band->min;
}

float Image::getBandMax(BandIndex b) const
{
    std::shared_ptr<Band> band = getBand(b);
    return band->max;
}

float Image::getBandsMin(BandIndices bands) const
{
    float min = std::numeric_limits<float>::max();
    for (int i = 0; i < c; i++) {
        std::shared_ptr<Band> band = getBand(bands[i]);
        if (band) {
            min = std::min(band->min, min);
        }
    }
    return min;
}

float Image::getBandsMax(BandIndices bands) const
{
    float max = std::numeric_limits<float>::lowest();
    for (int i = 0; i < c; i++) {
        std::shared_ptr<Band> band = getBand(bands[i]);
        if (band) {
            max = std::max(band->max, max);
        }
    }
    return max;
}

#include <gdal.h>
#include <gdal_priv.h>

class GDALChunkProvider : public ChunkProvider {
    GDALDataset* g;

public:

    GDALChunkProvider(GDALDataset* g) : g(g) {
    }

    virtual ~GDALChunkProvider() {
        GDALClose(g);
    }

    virtual void process(ChunkRequest cr, struct Image* image) {
        size_t x = cr.cx * CHUNK_SIZE;
        size_t y = cr.cy * CHUNK_SIZE;
        if (image->getBand(cr.bandidx)->getChunk(x, y))
            return;
        std::shared_ptr<Chunk> ck = std::make_shared<Chunk>();
        ck->w = std::min(CHUNK_SIZE, image->w - x);
        ck->h = std::min(CHUNK_SIZE, image->h - y);

        GDALRasterBand* band = g->GetRasterBand(cr.bandidx + 1);
        CPLErr err = band->RasterIO(GF_Read, x, y, ck->w, ck->h,
                                    &ck->pixels[0], ck->w, ck->h, GDT_Float32, 0, 0);
        if (err != CE_None) {
            std::cout << " err:" + std::to_string(err) << std::endl;
        }
        image->getBand(cr.bandidx)->setChunk(x, y, ck);
    }
};

std::shared_ptr<Image> create_image_from_filename(const std::string& filename)
{
    static int gdalinit = (GDALAllRegister(), 1);
    GDALDataset* g = (GDALDataset*) GDALOpen(filename.c_str(), GA_ReadOnly);
    if (!g)
        return nullptr;

    int w = g->GetRasterXSize();
    int h = g->GetRasterYSize();
    int d = g->GetRasterCount();
    std::shared_ptr<Image> image = std::make_shared<Image>(w, h, d);
    image->chunkProvider = std::make_shared<GDALChunkProvider>(g);
    return image;
}

