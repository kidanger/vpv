#include "GDALChunkSource.hpp"

#ifdef USE_GDAL

#include <algorithm>
#include <set>
#include <vector>

#include <cpl_error.h>
#include <gdal.h>
#include <gdal_priv.h>

GDALChunkSource::GDALChunkSource(GDALDataset* dataset, const std::string& filename, size_t w,
    size_t h, int rasterCount, bool complexAsTwoBands)
    : dataset(dataset)
    , w(w)
    , h(h)
    , rasterCount(rasterCount)
    , complexAsTwoBands(complexAsTwoBands)
    , filename(filename)
{
    tryThreadSafeReopen();
    buildLevels();
}

// A thread-safe handle can only be asked for at open time, so this reopens the
// file and drops the handle we were given. If anything about that fails we keep
// what we have and serialise reads through datasetMutex instead: the loader
// threads then still work in parallel across images, just not within one.
void GDALChunkSource::tryThreadSafeReopen()
{
#if defined(GDAL_OF_THREAD_SAFE) && GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 10, 0)
    if (filename.empty())
        return;

    CPLErrorReset();
    // quietly: a driver that cannot do this is not an error the user should see
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDataset* ts = (GDALDataset*)GDALOpenEx(filename.c_str(),
        GDAL_OF_RASTER | GDAL_OF_READONLY | GDAL_OF_THREAD_SAFE, nullptr, nullptr, nullptr);
    CPLPopErrorHandler();
    if (!ts)
        return;

    // Asking is not the same as getting: GDAL falls back to a plain handle when
    // the driver cannot support it.
    if (!GDALDatasetIsThreadSafe(ts, GDAL_OF_RASTER, nullptr)) {
        GDALClose(ts);
        return;
    }

    if (dataset) {
        GDALClose(dataset);
    }
    dataset = ts;
    threadSafe = true;
#endif
}

std::unique_lock<std::mutex> GDALChunkSource::lockDataset() const
{
    if (threadSafe) {
        // GDAL serialises what it has to internally; taking our own mutex here
        // would undo the whole point of the thread-safe handle
        return std::unique_lock<std::mutex>();
    }
    return std::unique_lock<std::mutex>(datasetMutex);
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

    auto lock = lockDataset();

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

// All the bands of one tile of one level go in one group. Not all of them end up
// in one *read* (see readBatch), but grouping them anyway keeps the bands of a
// tile from being scattered across loader threads, and is what step 12 extends to
// neighbouring tiles. The rest of the queue is left alone: merging positions is a
// different trade-off, and depends on the dataset's block geometry.
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
    // Only a complex band has anything to gain from being read as a group: its
    // two components are one GDAL band, so one read serves both, where two reads
    // would each decode the tile and throw half of it away.
    //
    // A real multi-band tile is *not* coalesced, although it could be with
    // GDALDataset::RasterIO and a band map. Measured on a 3-band pixel-
    // interleaved COG, 100 tiles of 1024^2 take 2497 ms band by band and 2476 ms
    // with a band map: nothing, because the driver already caches the sibling
    // blocks when one band is read. And a band map only exists on the dataset,
    // never on an overview, so it would have bought that nothing at level 0 only,
    // or at the price of one extra dataset handle per level. The loop below is
    // what those bands get, which is what they got before batching existed.
    bool sameTile = true;
    std::set<BandIndex> distinct;
    for (const ChunkKey& k : keys) {
        if (k.level != keys[0].level || k.cx != keys[0].cx || k.cy != keys[0].cy)
            sameTile = false;
        distinct.insert(k.band);
    }
    if (!complexAsTwoBands || keys.size() != 2 || !sameTile || distinct.size() != 2) {
        LazyChunkSource::readBatch(keys, out, errors);
        return;
    }

    const size_t level = keys[0].level;
    size_t x0, y0, cwidth, cheight;
    std::string error;
    if (!chunkWindow(level, keys[0].band, keys[0].cx, keys[0].cy, x0, y0, cwidth, cheight, error)) {
        LazyChunkSource::readBatch(keys, out, errors);
        return;
    }
    const size_t npix = cwidth * cheight;

    // Reused across calls: a batch is read over and over, on every loader thread.
    static thread_local std::vector<float> scratch;
    scratch.resize(npix * 2); // GDT_CFloat32: two floats per pixel

    auto lock = lockDataset();

    // the level's band 1; both components come from it
    GDALRasterBand* b = levelBand(level, 1, error);
    CPLErr err = CE_Failure;
    if (b) {
        CPLErrorReset();
        err = b->RasterIO(GF_Read, (int)x0, (int)y0, (int)cwidth, (int)cheight,
            scratch.data(), (int)cwidth, (int)cheight, GDT_CFloat32, 0, 0, nullptr);
        if (err != CE_None)
            error = lastGdalError();
    }

    if (err != CE_None) {
        // one window, one error: a partial failure fails the whole group, which is
        // then simply asked for again band by band the next time round
        for (size_t i = 0; i < keys.size(); i++) {
            out[i] = nullptr;
            errors[i] = error;
        }
        return;
    }

    for (size_t i = 0; i < keys.size(); i++) {
        const size_t component = keys[i].band; // 0 real, 1 imaginary
        out[i] = std::make_shared<Chunk>(cwidth, cheight);
        for (size_t p = 0; p < npix; p++) {
            out[i]->pixels[p] = scratch[p * 2 + component];
        }
    }
}

#endif
