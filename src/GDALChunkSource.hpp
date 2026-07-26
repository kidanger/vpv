#pragma once

#ifdef USE_GDAL

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ChunkLoader.hpp"

class GDALDataset;
class GDALRasterBand;

// Reads chunks straight out of a GDALDataset, on the chunk loading thread, one
// tile of one level at a time: all the bands of a tile that are wanted at once
// are read in a single call. See bigimages.md.
//
// Level 0 is the dataset itself; coarser levels are its overviews, in the order
// the driver reports them.
class GDALChunkSource : public LazyChunkSource {
    GDALDataset* dataset;
    // GDALDataset is not thread-safe. Today only the chunk loading thread
    // touches it, but this keeps that from being an accident.
    mutable std::mutex datasetMutex;

    size_t w, h;
    int rasterCount;
    // A single complex raster band is presented as two real bands, exactly like
    // the eager GDAL path does.
    bool complexAsTwoBands;

    std::vector<Level> levels;
    // GDAL overview index of each level > 0; levelOverview[level - 1]
    std::vector<int> levelOverview;

    void buildLevels();

public:
    // Takes ownership of 'dataset'.
    GDALChunkSource(GDALDataset* dataset, size_t w, size_t h, int rasterCount,
        bool complexAsTwoBands);
    ~GDALChunkSource() override;

    // Number of bands as seen by the rest of vpv.
    size_t bandCount() const { return complexAsTwoBands ? 2 : (size_t)rasterCount; }

    std::vector<Level> describeLevels() const override { return levels; }

protected:
    std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
        std::string& error) override;

    // Groups the queued reads that fall on the same tile of the same level, so
    // that all the bands of one tile are read together. See bigimages.md step 10.
    void planBatch(const std::vector<ChunkKey>& candidates, std::vector<ChunkKey>& group) override;
    void readBatch(const std::vector<ChunkKey>& keys, std::vector<std::shared_ptr<Chunk>>& out,
        std::vector<std::string>& errors) override;

private:
    // Geometry of a chunk, or false with 'error' set if there is no such chunk.
    bool chunkWindow(size_t level, BandIndex band, size_t cx, size_t cy,
        size_t& x0, size_t& y0, size_t& cw, size_t& ch, std::string& error) const;

    // The band of 'level' to read from, or nullptr with 'error' set. The dataset
    // mutex must be held.
    GDALRasterBand* levelBand(size_t level, int gdalband, std::string& error) const;
};

#endif
