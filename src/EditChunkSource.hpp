#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Chunk.hpp"
#include "ChunkLoader.hpp"
#include "editors.hpp"

struct Image;

// See bigimages.md, step 7.
//
// An edit whose inputs are not all resident cannot be evaluated whole, so it is
// evaluated one tile at a time, at whatever level is being asked for. Since
// fetch() already carries the level, the whole pyramid of an edited image lives
// in a single lazy Image and the collection cache key needs no level component.
//
// A tile is just a small image, so the program is run through the very same
// edit_images() as a whole image: the source assembles an interleaved buffer per
// input, evaluates, and slices the result back into chunks.
//
// Deliberate limitations, all documented in bigimages.md:
//  - no halo, so plambda's neighbourhood reads (x(-1,0), x,l ...) see the tile
//    border rather than the neighbouring tile, and magic variables (x%i) and
//    absolute coordinates (:i, :j) are tile-local;
//  - the geometry and the level list come from input 0, exactly as the
//    whole-image path takes w[0]/h[0]; the other inputs are read at the closest
//    level they have, with out-of-range coordinates clamped.
class EditChunkSource : public LazyChunkSource {
public:
    EditChunkSource(EditType edittype, std::string prog,
        std::vector<std::shared_ptr<Image>> inputs);

    // Evaluates one tile to discover the number of output bands, which nothing
    // else can tell us. Blocking; called from the provider before the Image
    // exists. Returns false and sets 'error' if the program cannot be run.
    bool probe(std::string& error);

    size_t bandCount() const { return outc; }

    std::vector<Level> describeLevels() const override { return levels; }

protected:
    std::shared_ptr<Chunk> read(size_t level, BandIndex band, size_t cx, size_t cy,
        std::string& error) override;

private:
    // Evaluates the tile and returns its bands, or an empty vector on failure.
    std::vector<std::shared_ptr<Chunk>> evalTile(size_t level, size_t cx, size_t cy,
        std::string& error);
    // The tile of one input, as an interleaved eager Image.
    std::shared_ptr<Image> assembleInput(size_t i, size_t level,
        size_t x0, size_t y0, size_t tw, size_t th) const;

    EditType edittype;
    std::string prog;
    std::vector<std::shared_ptr<Image>> inputs;
    std::vector<Level> levels;
    size_t outc = 0;

    // One evaluated tile, all its bands. The display asks for the three
    // displayed bands of the same tile back to back, and one evaluation produces
    // all of them; weak, because the chunks belong to ChunkCache like any other.
    mutable std::mutex memoMutex;
    bool memoValid = false;
    size_t memoLevel = 0, memoCx = 0, memoCy = 0;
    std::vector<std::weak_ptr<Chunk>> memoBands;
};
