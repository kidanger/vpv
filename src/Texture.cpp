#include <list>
#include <memory>

#ifndef SDL
#include <SFML/OpenGL.hpp>
#else
#include <GL/gl3w.h>
#endif

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
        casereturn(GL_OUT_OF_MEMORY);
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

static std::list<Tile> tileCache;

static void initTile(Tile t)
{
    GLuint internalFormat;
    switch (t.format) {
        case GL_RED:
            internalFormat = GL_R32F;
            break;
        case GL_RG:
            internalFormat = GL_RGB32F;
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

static Tile takeTile(size_t w, size_t h, unsigned format)
{
    for (auto it = tileCache.begin(); it != tileCache.end(); it++) {
        Tile t = *it;
        if (t.w == w && t.h == h && t.format == format) {
            tileCache.erase(it);
            return t;
        }
    }

    Tile tile;
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
    initTile(tile);
    return tile;
}

static void giveTile(Tile t)
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
        int _ts;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &_ts);
        GLDEBUG();
        ts = _ts;
        printf("maximum texture size: %lux%lu\n", ts, ts);
    }
    for (size_t y = 0; y < h; y += ts) {
        for (size_t x = 0; x < w; x += ts) {
            size_t tw = std::min(ts, w - x);
            size_t th = std::min(ts, h - y);
            Tile t = takeTile(tw, th, format);
            t.x = x;
            t.y = y;
            tiles.push_back(t);
        }
    }

    this->size.x = w;
    this->size.y = h;
    this->format = format;
}

void Texture::upload(const std::shared_ptr<Image>& img, ImRect area)
{
    unsigned int glformat;
    if (img->format == 1)
        glformat = GL_RED;
    else if (img->format == 2)
        glformat = GL_RG;
    else if (img->format == 3)
        glformat = GL_RGB;
    else if (img->format == 4)
        glformat = GL_RGBA;
    else
        assert(0);

    size_t w = img->w;
    size_t h = img->h;

    if (size.x != w || size.y != h || format != glformat) {
        create(w, h, glformat);
    }

    for (auto t : tiles) {
        ImRect intersect(t.x, t.y, t.x+t.w, t.y+t.h);
        intersect.ClipWithFull(area);
        ImRect totile = intersect;
        totile.Translate(ImVec2(-t.x, -t.y));

        if (intersect.GetWidth() == 0 || intersect.GetHeight() == 0) {
            continue;
        }

        const float* data = img->pixels + (w * (size_t)intersect.Min.y + (size_t)intersect.Min.x)*img->format;

        glBindTexture(GL_TEXTURE_2D, t.id);
        GLDEBUG();

        glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
        GLDEBUG();
        glTexSubImage2D(GL_TEXTURE_2D, 0, totile.Min.x, totile.Min.y,
                        totile.GetWidth(), totile.GetHeight(), glformat, GL_FLOAT, data);
        GLDEBUG();
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        GLDEBUG();

        if (gDownsamplingQuality >= 2) {
            glGenerateMipmap(GL_TEXTURE_2D);
            GLDEBUG();
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

Texture::~Texture() {
    for (auto t : tiles) {
        giveTile(t);
    }
    tiles.clear();
}

