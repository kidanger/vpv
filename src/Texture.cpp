#include <list>
#include <memory>

#include <GL/gl3w.h>

#include "Texture.hpp"
#include "Image.hpp"
#include "globals.hpp"

const char* getGLError(GLenum error)
{
#define casereturn(x) case x: return #x
    switch (error) {
        casereturn(GL_INVALID_ENUM);
        casereturn(GL_INVALID_VALUE);
        casereturn(GL_INVALID_OPERATION);
        casereturn(GL_INVALID_FRAMEBUFFER_OPERATION);
        case GL_OUT_OF_MEMORY: fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__); exit(1);
        default:
        case GL_NO_ERROR:
        return "";
    }
#undef casereturn
}

#define GLDEBUG(x) \
    x; \
{ \
    GLenum e; \
    while((e = glGetError()) != GL_NO_ERROR) \
    { \
        printf("%s:%s:%d for call %s\n", getGLError(e), __FILE__, __LINE__, #x); \
    } \
}

#define TEXTURE_MAX_SIZE (CHUNK_SIZE) //1024

static std::list<TextureTile> tileCache;

static void initTile(TextureTile t)
{
    GLuint internalFormat = GL_RGB32F;
    glBindTexture(GL_TEXTURE_2D, t.id);
    GLDEBUG();
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, t.w, t.h, 0, GL_RGB, GL_FLOAT, NULL);
    GLDEBUG();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLDEBUG();
    switch (gDownsamplingQuality) {
        case 0:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            break;
        case 1:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            break;
        case 2:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case 3:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            break;
    }
    GLDEBUG();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLDEBUG();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLDEBUG();

    glBindTexture(GL_TEXTURE_2D, 0);
    GLDEBUG();
}

static TextureTile takeTile(size_t w, size_t h)
{
    for (auto it = tileCache.begin(); it != tileCache.end(); it++) {
        TextureTile t = *it;
        if (t.w == w && t.h == h) {
            tileCache.erase(it);
            return t;
        }
    }

    TextureTile tile;
    if (!tileCache.empty()) {
        tile = tileCache.back();
        tileCache.pop_back();
    } else {
        glGenTextures(1, &tile.id);
        GLDEBUG();
    }
    tile.w = w;
    tile.h = h;
    initTile(tile);
    return tile;
}

static void giveTile(TextureTile t)
{
    tileCache.push_back(t);
}

void Texture::upload(const std::shared_ptr<Image>& img, ImRect area, BandIndices bandidx)
{
    static float* data = new float[TEXTURE_MAX_SIZE*TEXTURE_MAX_SIZE*3];
    static float* zeros = nullptr;
    if (!zeros) {
        zeros = new float[CHUNK_SIZE*CHUNK_SIZE];
        memset(zeros, 0, sizeof(float)*CHUNK_SIZE*CHUNK_SIZE);
    }

    size_t w = img->w;
    size_t h = img->h;

    if (size.x != w || size.y != h || currentImage != img || currentBands != bandidx) {
        for (auto& tt : tiles) {
            for (auto& t : tt) {
                if (t) giveTile(*t);
            }
        }
        tiles.clear();

        size.x = w;
        size.y = h;
        currentImage = img;
        currentBands = bandidx;

        size_t cw = w / CHUNK_SIZE + 1;
        size_t ch = h / CHUNK_SIZE + 1;
        tiles.resize(cw);
        for (int i = 0; i < cw; i++) {
            tiles[i].resize(ch);
        }
    }

    ImVec2 p1 = area.Min;
    ImVec2 p2 = area.Max;
    visibility.clear();
    for (size_t y = p1.y; y < p2.y+CHUNK_SIZE && y < h; y+=CHUNK_SIZE) {
        for (size_t x = p1.x; x < p2.x+CHUNK_SIZE && x < w; x+=CHUNK_SIZE) {
            visibility.push_back(std::make_pair(x / CHUNK_SIZE, y / CHUNK_SIZE));
        }
    }

    for (auto p : visibility) {
        size_t x = p.first;
        size_t y = p.second;
        nonstd::optional<TextureTile>& t = tiles[x][y];

        if (t)
            continue;

        std::shared_ptr<Chunk> cks[3]; // only used to keep the chunks allocated
        float* bands[3] = {0, 0, 0};
        size_t cw, ch;
        for (int c = 0; c < 3; c++) {
            size_t b = bandidx[c];
            const std::shared_ptr<Band> band = img->getBand(b);
            if (!band) {
                bands[c] = zeros;
                continue;
            }

            std::shared_ptr<Chunk> ck = band->getChunk(x * CHUNK_SIZE, y * CHUNK_SIZE);
            if (!ck) {
                img->requestChunkAtBand(b, x * CHUNK_SIZE, y * CHUNK_SIZE);
                continue;
            }

            cks[c] = ck;
            bands[c] = &ck->pixels[0];
            cw = ck->w;
            ch = ck->h;
        }

        if (!bands[0] || !bands[1] || !bands[2]
            || (bands[0] == zeros && bands[1] == zeros && bands[2] == zeros)) {
            continue;
        }

        size_t tw = std::min(CHUNK_SIZE, w - x);
        size_t th = std::min(CHUNK_SIZE, h - y);
        t = takeTile(tw, th);
        t->x = x * CHUNK_SIZE;
        t->y = y * CHUNK_SIZE;

        float* rs = bands[0];
        float* gs = bands[1];
        float* bs = bands[2];
        int idx = 0;
        for (int y = 0; y < ch; y++) {
            for (int x = 0; x < cw; x++) {
                float r = rs[y*cw+x];
                float g = gs[y*cw+x];
                float b = bs[y*cw+x];
                data[idx++] = r;
                data[idx++] = g;
                data[idx++] = b;
            }
        }

        GLDEBUG();
        glBindTexture(GL_TEXTURE_2D, t->id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t->w, t->h, GL_RGB, GL_FLOAT, data);
        GLDEBUG();

        if (gDownsamplingQuality >= 2) {
            glGenerateMipmap(GL_TEXTURE_2D);
            GLDEBUG();
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        GLDEBUG();
    }
}

Texture::~Texture()
{
    for (auto& tt : tiles) {
        for (auto t : tt) {
            if (t) giveTile(*t);
        }
    }
    tiles.clear();
}

