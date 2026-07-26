#include "GDALChunkSource.hpp"

#ifdef USE_GDAL

#include <algorithm>
#include <set>
#include <vector>

#include <cpl_error.h>
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

bool GDALChunkSource::chunkWindow(size_t level, BandIndex band, size_t cx, size_t cy,
    size_t& x0, size_t& y0, size_t& cw, size_t& ch, std::string& error) const
{
    if (level >= levels.size() || band >= bandCount()) {
        error = "no such band";
        return false;
    }

    const Level& lv = levels[level];
    x0 = cx * CHUNK_SIZE;
    y0 = cy * CHUNK_SIZE;
    if (x0 >= lv.w || y0 >= lv.h) {
        error = "chunk outside the raster";
        return false;
    }

    cw = std::min(CHUNK_SIZE, lv.w - x0);
    ch = std::min(CHUNK_SIZE, lv.h - y0);
    return true;
}

GDALRasterBand* GDALChunkSource::levelBand(size_t level, int gdalband, std::string& error) const
{
    if (!dataset) {
        error = "the dataset is closed";
        return nullptr;
    }
    GDALRasterBand* b = dataset->GetRasterBand(gdalband);
    if (!b) {
        error = "no such raster band";
        return nullptr;
    }
    if (level > 0) {
        b = b->GetOverview(levelOverview[level - 1]);
        if (!b) {
            error = "the overview disappeared";
            return nullptr;
        }
    }
    return b;
}

// Whatever GDAL logged for the read that just failed; it is the only useful
// thing we can report, and it is what the indicator's tooltip shows.
static std::string lastGdalError()
{
    const char* msg = CPLGetLastErrorMsg();
    return (msg && *msg) ? msg : "GDAL could not read the block";
}

std::shared_ptr<Chunk> GDALChunkSource::read(size_t level, BandIndex band, size_t cx, size_t cy,
    std::string& error)
{
    size_t x0, y0, cwidth, cheight;
    if (!chunkWindow(level, band, cx, cy, x0, y0, cwidth, cheight, error))
        return nullptr;

    auto chunk = std::make_shared<Chunk>(cwidth, cheight);

    std::lock_guard<std::mutex> lock(datasetMutex);

    // for complex data, band 0 and 1 are the two components of raster band 1
    int gdalband = complexAsTwoBands ? 1 : (int)band + 1;
    GDALRasterBand* b = levelBand(level, gdalband, error);
    if (!b)
        return nullptr;

    CPLErrorReset();

    CPLErr err;
    if (complexAsTwoBands) {
        // GDT_CFloat32 gives two floats per pixel; keep the component we want.
        // Reading a single component of a complex tile costs the whole tile, which
        // is why readBatch() serves both at once whenever both are wanted.
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

    if (err != CE_None) {
        error = lastGdalError();
        return nullptr;
    }

    return chunk;
}

// All the bands of one tile of one level, in one call. The rest of the queue is
// left alone: merging *positions* is a different trade-off, and depends on the
// dataset's block geometry (step 12).
void GDALChunkSource::planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group)
{
    const ChunkKey& first = candidates[0];
    for (const ChunkKey& k : candidates) {
        if (k.level == first.level && k.cx == first.cx && k.cy == first.cy) {
            group.push_back(k);
        }
    }
}

void GDALChunkSource::readBatch(const std::vector<ChunkKey>& keys,
    std::vector<std::shared_ptr<Chunk>>& out, std::vector<std::string>& errors)
{
    // Nothing to coalesce, or a group we did not plan: the one-by-one path is
    // always correct.
    bool sameTile = true;
    std::set<BandIndex> distinct;
    for (const ChunkKey& k : keys) {
        if (k.level != keys[0].level || k.cx != keys[0].cx || k.cy != keys[0].cy)
            sameTile = false;
        distinct.insert(k.band);
    }
    if (keys.size() < 2 || !sameTile || distinct.size() != keys.size()) {
        LazyChunkSource::readBatch(keys, out, errors);
        return;
    }

    const size_t level = keys[0].level;
    size_t x0, y0, cwidth, cheight;
    {
        std::string error;
        if (!chunkWindow(level, keys[0].band, keys[0].cx, keys[0].cy, x0, y0, cwidth, cheight,
                error)) {
            LazyChunkSource::readBatch(keys, out, errors);
            return;
        }
    }
    const size_t npix = cwidth * cheight;

    // A real multi-band read needs a band map, which only GDALDataset::RasterIO
    // takes and which has no overview equivalent, so above level 0 the per-band
    // path stays. That costs nothing: the repeat cost of an overview block is
    // what GDAL's own block cache absorbs.
    if (!complexAsTwoBands && level > 0) {
        LazyChunkSource::readBatch(keys, out, errors);
        return;
    }

    for (size_t i = 0; i < keys.size(); i++) {
        out[i] = std::make_shared<Chunk>(cwidth, cheight);
    }

    // Band-sequential, so each band comes out as one contiguous plane that can be
    // copied straight into its chunk. Reused across calls because a batch is read
    // on every loader thread, over and over.
    static thread_local std::vector<float> scratch;
    // complex is one interleaved read of two components; real is one plane per band
    scratch.resize(complexAsTwoBands ? npix * 2 : npix * keys.size());

    std::lock_guard<std::mutex> lock(datasetMutex);

    CPLErrorReset();
    CPLErr err;
    std::string error;

    if (complexAsTwoBands) {
        // The whole point: one GDT_CFloat32 read of raster band 1 fills both
        // pseudo-bands, where two separate reads would each decode the tile and
        // throw half of it away.
        GDALRasterBand* b = levelBand(level, 1, error);
        if (!b) {
            for (size_t i = 0; i < keys.size(); i++) {
                out[i] = nullptr;
                errors[i] = error;
            }
            return;
        }
        err = b->RasterIO(GF_Read, x0, y0, cwidth, cheight,
            scratch.data(), cwidth, cheight, GDT_CFloat32, 0, 0, nullptr);
        if (err == CE_None) {
            for (size_t i = 0; i < keys.size(); i++) {
                const size_t component = keys[i].band; // 0 real, 1 imaginary
                for (size_t p = 0; p < npix; p++) {
                    out[i]->pixels[p] = scratch[p * 2 + component];
                }
            }
        }
    } else {
        if (!dataset) {
            for (size_t i = 0; i < keys.size(); i++) {
                out[i] = nullptr;
                errors[i] = "the dataset is closed";
            }
            return;
        }
        std::vector<int> bandMap;
        for (const ChunkKey& k : keys) {
            bandMap.push_back((int)k.band + 1);
        }
        err = dataset->RasterIO(GF_Read, (int)x0, (int)y0, (int)cwidth, (int)cheight,
            scratch.data(), (int)cwidth, (int)cheight, GDT_Float32,
            (int)bandMap.size(), bandMap.data(),
            0, 0, (GSpacing)(npix * sizeof(float)), nullptr);
        if (err == CE_None) {
            for (size_t i = 0; i < keys.size(); i++) {
                std::copy(scratch.begin() + i * npix, scratch.begin() + (i + 1) * npix,
                    out[i]->pixels.begin());
            }
        }
    }

    if (err != CE_None) {
        // one window, one error: a partial failure fails the whole group, which
        // then simply gets asked for again band by band the next time round
        error = lastGdalError();
        for (size_t i = 0; i < keys.size(); i++) {
            out[i] = nullptr;
            errors[i] = error;
        }
    }
}

#endif
