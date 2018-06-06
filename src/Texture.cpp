#ifndef SDL
#include <SFML/OpenGL.hpp>
#else
#include <GL/gl3w.h>
#endif

#include "Texture.hpp"
#include "Image.hpp"
#include "globals.hpp"

#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif

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

static GLuint createTexture(int w, int h, int format)
{
    GLuint id;
    glGenTextures(1, &id);
    GLDEBUG();

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, format, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

void Texture::create(int w, int h, unsigned int format)
{
    static int ts = 0;
    if (!ts) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ts);
        ts /= 2;  // to avoid a strange i965 bug
        printf("maximum texture size: %dx%d\n", ts, ts);
    }
    for (int y = 0; y < h; y += ts) {
        for (int x = 0; x < w; x += ts) {
            Tile t;
            t.x = x;
            t.y = y;
            t.w = std::min(ts, w - x);
            t.h = std::min(ts, h - y);
            t.id = createTexture(t.w, t.h, format);
            tiles.push_back(t);
        }
    }

    size.x = w;
    size.y = h;
    this->format = format;
}

void Texture::upload(const Image* img, ImRect area)
{
    unsigned int glformat;
    if (img->format == Image::R)
        glformat = GL_RED;
    else if (img->format == Image::RG)
        glformat = GL_RG;
    else if (img->format == Image::RGB)
        glformat = GL_RGB;
    else if (img->format == Image::RGBA)
        glformat = GL_RGBA;
    else
        assert(0);

    int w = img->w;
    int h = img->h;

    if (size.x != w || size.y != h || format != glformat) {
        create(w, h, glformat);
    }

    for (auto t : tiles) {
         const float* data = img->pixels + (w * t.y + t.x)*img->format;

         glBindTexture(GL_TEXTURE_2D, t.id);
         GLDEBUG();

         glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
         GLDEBUG();
         glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t.w, t.h, glformat, GL_FLOAT, data);
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
        glDeleteTextures(1, &t.id);
    }
    tiles.clear();
}

