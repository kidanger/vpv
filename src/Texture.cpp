#include <list>
#include <memory>

#include <GL/gl3w.h>

#include "Texture.hpp"
#include "Image.hpp"
#include "globals.hpp"
#include "OpenGLDebug.hpp"

#define TEXTURE_MAX_SIZE 1024

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
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, t.w, t.h, 0, t.format, GL_FLOAT, nullptr);
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
    GLDEBUG();
    bool needsreshape = bandidx[0] != 0 || bandidx[1] != 1 || bandidx[2] != 2 || img->c > 3;
    unsigned int glformat = GL_RGB;
    if (!needsreshape) {
        if (img->c == 1)
            glformat = GL_RED;
        else if (img->c == 2)
            glformat = GL_RG;
        else if (img->c == 3)
            glformat = GL_RGB;
    }

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

        const float* data;
        if (!needsreshape) {
            data = img->pixels + (w * (size_t)intersect.Min.y + (size_t)intersect.Min.x)*img->c;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
        } else {
            // NOTE: all this copy and upload is slow
            // 1) use opengl buffer to avoid pausing at each tile's upload
            // 2° prepare the reshapebuffers in a thread
            // storing these images as planar would help with cache
            static float* reshapebuffer = new float[TEXTURE_MAX_SIZE*TEXTURE_MAX_SIZE*3];
            for (int c = 0; c < 3; c++) {
                size_t b = bandidx[c];
                if (b >= img->c) {
                    for (int y = 0; y < t.h; y++) {
                        for (int x = 0; x < t.w; x++) {
                            reshapebuffer[(y*TEXTURE_MAX_SIZE+x)*3+c] = 0;
                        }
                    }
                    continue;
                }
                int sx = intersect.Min.x;
                int sy = intersect.Min.y;
                for (int y = 0; y < intersect.GetHeight(); y++) {
                    for (int x = 0; x < intersect.GetWidth(); x++) {
                        float v = img->pixels[((sy+y)*img->w+sx+x)*img->c+b];
                        reshapebuffer[(y*TEXTURE_MAX_SIZE+x)*3+c] = v;
                    }
                }
            }
            data = reshapebuffer;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, TEXTURE_MAX_SIZE);
        }

        glBindTexture(GL_TEXTURE_2D, t.id);
        GLDEBUG();

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
        GLDEBUG();
    }
}

Texture::~Texture()
{
    for (auto t : tiles) {
        giveTile(t);
    }
    tiles.clear();
}

