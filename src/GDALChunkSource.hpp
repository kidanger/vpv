#pragma once

#ifdef USE_GDAL

#include <memory>
#include <mutex>
#include <string>

#include "ChunkLoader.hpp"

class GDALDataset;

// Reads chunks straight out of a GDALDataset, one at a time, on the chunk
// loading thread. See bigimages.md.
//
// Step 3 only exposes level 0; the overviews become extra levels in step 4.
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

public:
    // Takes ownership of 'dataset'.
    GDALChunkSource(GDALDataset* dataset, size_t w, size_t h, int rasterCount,
        bool complexAsTwoBands);
    ~GDALChunkSource() override;

    // Number of bands as seen by the rest of vpv.
    size_t bandCount() const { return complexAsTwoBands ? 2 : (size_t)rasterCount; }

protected:
    std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy) override;
};

#endif
