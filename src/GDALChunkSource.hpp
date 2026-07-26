#pragma once

#ifdef USE_GDAL

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ChunkLoader.hpp"

class GDALDataset;
class GDALRasterBand;

// Reads chunks straight out of a GDALDataset, on the chunk loading threads, one
// tile of one level at a time. The two components of a complex band are read
// together, in a single call.
//
// Whether those threads read this dataset concurrently or one at a time depends
// on whether GDAL could give a thread-safe handle for the file: see
// tryThreadSafeReopen. See bigimages.md.
//
// Level 0 is the dataset itself; coarser levels are its overviews, in the order
// the driver reports them.
class GDALChunkSource : public LazyChunkSource {
    GDALDataset* dataset;
    // GDALDataset is not thread-safe in general, so every read of it is
    // serialised... unless GDAL gave us a thread-safe handle (GDAL_OF_THREAD_SAFE,
    // 3.10+), in which case the loader threads read it concurrently and this is
    // never taken. See bigimages.md step 11.
    mutable std::mutex datasetMutex;
    bool threadSafe = false;

    size_t w, h;
    int rasterCount;
    // A single complex raster band is presented as two real bands, exactly like
    // the eager GDAL path does.
    bool complexAsTwoBands;
    // What the dataset was opened from, so that it can be reopened thread-safe.
    std::string filename;

    std::vector<Level> levels;
    // GDAL overview index of each level > 0; levelOverview[level - 1]
    std::vector<int> levelOverview;

    void buildLevels();
    // Swaps 'dataset' for a thread-safe handle on the same file, if GDAL can give
    // one. Runs once, at construction.
    void tryThreadSafeReopen();

    // Held for the duration of a read, unless the handle is thread-safe.
    std::unique_lock<std::mutex> lockDataset() const;

public:
    // Takes ownership of 'dataset'. 'filename' is what it was opened from.
    GDALChunkSource(GDALDataset* dataset, const std::string& filename, size_t w, size_t h,
        int rasterCount, bool complexAsTwoBands);
    ~GDALChunkSource() override;

    // Whether reads of this source run in parallel, for the HUD.
    bool isThreadSafe() const { return threadSafe; }

    // Number of bands as seen by the rest of vpv.
    size_t bandCount() const { return complexAsTwoBands ? 2 : (size_t)rasterCount; }

    std::vector<Level> describeLevels() const override { return levels; }

protected:
    std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
        std::string& error) override;

    // Groups the queued reads that fall on the same tile of the same level.
    // readBatch() then serves a complex tile's two components in one call; the
    // other bands are read one by one, for the reasons given there.
    void planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group) override;
    void readBatch(const std::vector<ChunkKey>& keys, std::vector<std::shared_ptr<Chunk>>& out,
        std::vector<std::string>& errors) override;

    // As many as there are loader threads when GDAL gave us a thread-safe handle;
    // one otherwise, since datasetMutex would serialise them anyway and a thread
    // waiting on it is a thread not reading another image.
    size_t maxConcurrentReads() const override { return threadSafe ? (size_t)-1 : 1; }

private:
    // Geometry of a chunk, or false with 'error' set if there is no such chunk.
    bool chunkWindow(size_t level, BandIndex band, size_t cx, size_t cy,
        size_t& x0, size_t& y0, size_t& cw, size_t& ch, std::string& error) const;

    // The band of 'level' to read from, or nullptr with 'error' set. The dataset
    // mutex must be held.
    GDALRasterBand* levelBand(size_t level, int gdalband, std::string& error) const;
};

#endif
