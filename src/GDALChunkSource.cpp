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
    buildLevels();
}

GDALChunkSource::~GDALChunkSource()
{
    if (dataset) {
        GDALClose(dataset);
    }
}

// Turn the dataset's overviews into levels. Runs once, at construction, before
// anybody else can see the source.
void GDALChunkSource::buildLevels()
{
    levels.emplace_back(w, h, 1.0, 1.0);

    GDALRasterBand* first = dataset ? dataset->GetRasterBand(1) : nullptr;
    if (!first)
        return;

    int count = first->GetOverviewCount();
    for (int i = 0; i < count; i++) {
        GDALRasterBand* ovr = first->GetOverview(i);
        if (!ovr)
            break;
        size_t ow = (size_t)ovr->GetXSize();
        size_t oh = (size_t)ovr->GetYSize();
        // The driver is not required to report them sorted, and a degenerate
        // overview would break the "levels get coarser" assumption everything
        // else relies on.
        if (ow == 0 || oh == 0 || ow >= levels.back().w || oh >= levels.back().h)
            continue;

        // Every band must have the same overview, otherwise a level would exist
        // for some bands only. Stop at the first level that is not shared.
        bool shared = true;
        for (int b = 2; b <= rasterCount && shared; b++) {
            GDALRasterBand* other = dataset->GetRasterBand(b);
            GDALRasterBand* otherOvr = other ? other->GetOverview(i) : nullptr;
            shared = otherOvr && (size_t)otherOvr->GetXSize() == ow
                && (size_t)otherOvr->GetYSize() == oh;
        }
        if (!shared)
            break;

        levels.emplace_back(ow, oh, (double)w / ow, (double)h / oh);
        levelOverview.push_back(i);
    }
}

std::shared_ptr<Chunk> GDALChunkSource::read(size_t level, BandIndex band, size_t cx, size_t cy)
{
    if (level >= levels.size() || band >= bandCount())
        return nullptr;

    const Level& lv = levels[level];
    size_t x0 = cx * CHUNK_SIZE;
    size_t y0 = cy * CHUNK_SIZE;
    if (x0 >= lv.w || y0 >= lv.h)
        return nullptr;

    size_t cwidth = std::min(CHUNK_SIZE, lv.w - x0);
    size_t cheight = std::min(CHUNK_SIZE, lv.h - y0);

    auto chunk = std::make_shared<Chunk>(cwidth, cheight);

    std::lock_guard<std::mutex> lock(datasetMutex);
    if (!dataset)
        return nullptr;

    // for complex data, band 0 and 1 are the two components of raster band 1
    int gdalband = complexAsTwoBands ? 1 : (int)band + 1;
    GDALRasterBand* b = dataset->GetRasterBand(gdalband);
    if (!b)
        return nullptr;
    if (level > 0) {
        b = b->GetOverview(levelOverview[level - 1]);
        if (!b)
            return nullptr;
    }

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
