#include "Chunk.hpp"

std::shared_ptr<Chunk> InRamChunkSource::fetch(size_t level, BandIndex band, size_t cx, size_t cy)
{
    if (level != 0 || band >= c)
        return nullptr;

    size_t x0 = cx * CHUNK_SIZE;
    size_t y0 = cy * CHUNK_SIZE;
    if (x0 >= w || y0 >= h)
        return nullptr;

    size_t cwidth = std::min(CHUNK_SIZE, w - x0);
    size_t cheight = std::min(CHUNK_SIZE, h - y0);

    auto chunk = std::make_shared<Chunk>(cwidth, cheight);
    for (size_t y = 0; y < cheight; y++) {
        const float* src = pixels + ((y0 + y) * w + x0) * c + band;
        float* dst = &chunk->pixels[y * cwidth];
        for (size_t x = 0; x < cwidth; x++) {
            dst[x] = src[x * c];
        }
    }
    return chunk;
}

#include <doctest.h>

TEST_CASE("InRamChunkSource::fetch")
{
    // a 3x2 image with 2 bands, interleaved
    std::vector<float> pixels {
        0.f, 100.f, 1.f, 101.f, 2.f, 102.f,
        3.f, 103.f, 4.f, 104.f, 5.f, 105.f
    };
    InRamChunkSource src(pixels.data(), 3, 2, 2);

    SUBCASE("band 0")
    {
        auto c = src.fetch(0, 0, 0, 0);
        REQUIRE(bool(c));
        CHECK(c->w == 3);
        CHECK(c->h == 2);
        CHECK(c->pixels == std::vector<float> { 0.f, 1.f, 2.f, 3.f, 4.f, 5.f });
    }

    SUBCASE("band 1")
    {
        auto c = src.fetch(0, 1, 0, 0);
        REQUIRE(bool(c));
        CHECK(c->pixels == std::vector<float> { 100.f, 101.f, 102.f, 103.f, 104.f, 105.f });
    }

    SUBCASE("out of range")
    {
        CHECK(bool(src.fetch(0, 2, 0, 0)) == false); // no such band
        CHECK(bool(src.fetch(1, 0, 0, 0)) == false); // no such level
        CHECK(bool(src.fetch(0, 0, 1, 0)) == false); // no such chunk
    }
}

TEST_CASE("InRamChunkSource::fetch clips border chunks")
{
    size_t w = CHUNK_SIZE + 3, h = CHUNK_SIZE + 1;
    std::vector<float> pixels(w * h, 7.f);
    InRamChunkSource src(pixels.data(), w, h, 1);

    Level level(w, h, 1.0);
    CHECK(level.cw() == 2);
    CHECK(level.ch() == 2);

    auto topleft = src.fetch(0, 0, 0, 0);
    REQUIRE(bool(topleft));
    CHECK(topleft->w == CHUNK_SIZE);
    CHECK(topleft->h == CHUNK_SIZE);

    auto bottomright = src.fetch(0, 0, 1, 1);
    REQUIRE(bool(bottomright));
    CHECK(bottomright->w == 3);
    CHECK(bottomright->h == 1);
    CHECK(bottomright->w == level.chunkWidth(1));
    CHECK(bottomright->h == level.chunkHeight(1));
}
