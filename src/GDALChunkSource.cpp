#include "GDALChunkSource.hpp"

#ifdef USE_GDAL

#include <algorithm>
#include <vector>

#include <gdal.h>
#include <gdal_priv.h>

GDALChunkSource::GDALChunkSource(GDALDataset* dataset, size_t w, size_t h, int rasterCount,
    bool complexAsTwoBands)
    : dataset(dataset)
    , w(w)
    , h(h)
    , rasterCount(rasterCount)
    , complexAsTwoBands(complexAsTwoBands)
{
}

GDALChunkSource::~GDALChunkSource()
{
    if (dataset) {
        GDALClose(dataset);
    }
}

std::shared_ptr<Chunk> GDALChunkSource::read(size_t level, BandIndex band, size_t cx, size_t cy)
{
    if (level != 0 || band >= bandCount())
        return nullptr;

    size_t x0 = cx * CHUNK_SIZE;
    size_t y0 = cy * CHUNK_SIZE;
    if (x0 >= w || y0 >= h)
        return nullptr;

    size_t cwidth = std::min(CHUNK_SIZE, w - x0);
    size_t cheight = std::min(CHUNK_SIZE, h - y0);

    auto chunk = std::make_shared<Chunk>(cwidth, cheight);

    std::lock_guard<std::mutex> lock(datasetMutex);
    if (!dataset)
        return nullptr;

    // for complex data, band 0 and 1 are the two components of raster band 1
    int gdalband = complexAsTwoBands ? 1 : (int)band + 1;
    GDALRasterBand* b = dataset->GetRasterBand(gdalband);
    if (!b)
        return nullptr;

    CPLErr err;
    if (complexAsTwoBands) {
        // GDT_CFloat32 gives two floats per pixel; keep the component we want
        std::vector<float> tmp(cwidth * cheight * 2);
        err = b->RasterIO(GF_Read, x0, y0, cwidth, cheight,
            tmp.data(), cwidth, cheight, GDT_CFloat32,
            0, 0, nullptr);
        if (err == CE_None) {
            for (size_t i = 0; i < cwidth * cheight; i++) {
                chunk->pixels[i] = tmp[i * 2 + band];
            }
        }
    } else {
        err = b->RasterIO(GF_Read, x0, y0, cwidth, cheight,
            chunk->pixels.data(), cwidth, cheight, GDT_Float32,
            0, 0, nullptr);
    }

    if (err != CE_None)
        return nullptr;

    return chunk;
}

#endif
