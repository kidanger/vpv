#pragma once

#include <cstddef>
#include <memory>

struct Chunk;
struct Image;

// See bigimages.md.
//
// The one owner of resident chunks. An Image's residency grid only holds
// weak_ptrs, so dropping a chunk here is all it takes to evict it: nothing has
// to be called back into, nothing can dangle, and a caller that is holding a
// shared_ptr to the chunk keeps it alive until it lets go.
//
// Chunks share CACHE_LIMIT with the whole-image cache, because both are just
// pixels in RAM. ImageCache reports its own total here so that there is a single
// budget and a single place that knows the answer.
namespace ChunkCache {

// Take (or refresh) ownership of a chunk read on behalf of 'owner', and evict
// whatever has to go. Idempotent: calling it for a chunk that is already held
// just marks it as used, and updates its owner, which is how a chunk that was
// read speculatively by the loader (owner == nullptr) is adopted by the Image
// that ends up claiming it.
//
// Never called with any of Image's locks held.
void retain(const std::shared_ptr<Chunk>& chunk, const Image* owner);

// Drop everything owned by 'owner'. Called when an Image dies: without it, its
// chunks would sit in the cache until some other image's reads pushed them out.
void forget(const Image* owner);

// Called once per frame from the main loop. Chunks touched during the current
// frame are never evicted: the display asks for its visible chunks every frame,
// so evicting one would have it read again immediately, forever. If the visible
// set alone does not fit in the budget we go over budget for that frame instead
// of thrashing.
void beginFrame();

// Bytes of resident chunks.
size_t bytes();

// Bytes held by the whole-image cache. It is the one that knows; we are the one
// that has to fit underneath the same limit.
void setImageBytes(size_t bytes);
size_t totalBytes();

void flush();

// Resident set size of the whole process, or 0 where we cannot tell. Not part of
// any budget: it is here to tell "the cache is not evicting" apart from "the
// memory is somebody else's" (GDAL's block cache, the allocator holding on to
// freed chunks), which is the only way to diagnose growth by hand.
size_t processBytes();

// What GDAL is currently holding in its own cache of decoded raster blocks, or 0
// when vpv was built without GDAL. Also a diagnostic, not part of our budget:
// GDAL_CACHE_LIMIT is the ceiling, this is the fill level. It lives here rather
// than next to GDALChunkSource so that the HUD has one place to ask about
// memory.
size_t gdalCacheBytes();

}
