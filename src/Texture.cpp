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
    GLuint internalFormat;
    switch (t.format) {
        case GL_RED:
            internalFormat = GL_R32F;
            break;
        case GL_RG:
            internalFormat = GL_RG32F;
            break;
        case GL_RGB:
            internalFormat = GL_RGB32F;
            break;
        case GL_RGBA:
            internalFormat = GL_RGBA32F;
            break;
        default:
            assert(0);
    }

    glBindTexture(GL_TEXTURE_2D, t.id);
    GLDEBUG();
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, t.w, t.h, 0, t.format, GL_FLOAT, NULL);
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

static TextureTile takeTile(size_t w, size_t h, unsigned format)
{
    for (auto it = tileCache.begin(); it != tileCache.end(); it++) {
        TextureTile t = *it;
        if (t.w == w && t.h == h && t.format == format) {
            tileCache.erase(it);
            t.state = TextureTile::VOID;
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
    tile.format = format;
    tile.state = TextureTile::VOID;
    initTile(tile);
    return tile;
}

static void giveTile(TextureTile t)
{
    tileCache.push_back(t);
}

void Texture::create(size_t w, size_t h, unsigned format)
{
    for (auto t : tiles) {
        giveTile(t);
    }
    tiles.clear();

    static size_t ts = 0;
    if (!ts) {
        GLDEBUG();
        int _ts;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &_ts);
        GLDEBUG();
        ts = _ts;
        ts = TEXTURE_MAX_SIZE;
    }
    for (size_t y = 0; y < h; y += ts) {
        for (size_t x = 0; x < w; x += ts) {
            size_t tw = std::min(ts, w - x);
            size_t th = std::min(ts, h - y);
            TextureTile t = takeTile(tw, th, format);
            t.x = x;
            t.y = y;
            tiles.push_back(t);
        }
    }

    this->size.x = w;
    this->size.y = h;
    this->format = format;
}

void Texture::upload(const std::shared_ptr<Image>& img, ImRect area, BandIndices bandidx)
{
    static float* zeros = nullptr;
    if (!zeros) {
        zeros = new float[CHUNK_SIZE*CHUNK_SIZE];
        memset(zeros, 0, sizeof(float)*CHUNK_SIZE*CHUNK_SIZE);
    }

    size_t w = img->w;
    size_t h = img->h;

    if (size.x != w || size.y != h || currentImage != img || currentBands != bandidx) {
        create(w, h, GL_RGB);
        currentImage = img;
        currentBands = bandidx;
    }

    for (auto& t : tiles) {
        if (t.state == TextureTile::READY)
            continue;

        ImRect intersect(t.x, t.y, t.x+t.w, t.y+t.h);
        intersect.ClipWithFull(area);
        ImRect totile = intersect;
        totile.Translate(ImVec2(-t.x, -t.y));

        if (intersect.GetWidth() == 0 || intersect.GetHeight() == 0) {
            continue;
        }

        // NOTE: all this copy and upload is slow
        // 1) use opengl buffer to avoid pausing at each tile's upload
        // 2) prepare the reshapebuffers in a thread
        // storing these images as planar would help with cache
        static float* data = new float[TEXTURE_MAX_SIZE*TEXTURE_MAX_SIZE*3];
        //memset(data, 0, sizeof(float)*TEXTURE_MAX_SIZE*TEXTURE_MAX_SIZE*3);

        // check whether we have a chunk for each band
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

            std::shared_ptr<Chunk> ck = band->getChunk(t.x, t.y);
            if (!ck) {
                img->requestChunkAtBand(b, t.x, t.y);
                // TODO: display that the chunk is not loaded with a nice shader
                continue;
            }

            cks[c] = ck;
            bands[c] = &ck->pixels[0];
            cw = ck->w;
            ch = ck->h;
        }

        if (!bands[0] || !bands[1] || !bands[2]
            || (bands[0] == zeros && bands[1] == zeros && bands[2] == zeros)) {
            t.state = TextureTile::LOADING;
            continue;
        }

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
        glBindTexture(GL_TEXTURE_2D, t.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t.w, t.h, t.format, GL_FLOAT, data);
        GLDEBUG();

        if (gDownsamplingQuality >= 2) {
            glGenerateMipmap(GL_TEXTURE_2D);
            GLDEBUG();
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        GLDEBUG();
        t.state = TextureTile::READY;
    }
}

Texture::~Texture()
{
    for (auto t : tiles) {
        giveTile(t);
    }
    tiles.clear();
}

