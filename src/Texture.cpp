#ifndef SDL
#include <SFML/OpenGL.hpp>
#else
#include <GL/gl3w.h>
#endif

#include "Texture.hpp"
#include "globals.hpp"

#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif

void Texture::create(int w, int h, unsigned int type, unsigned int format)
{
    if (id == -1) {
        glGenTextures(1, &id);
    }

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, format, type, NULL);

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

    size.x = w;
    size.y = h;
    this->type = type;
    this->format = format;
}

Texture::~Texture() {
    glDeleteTextures(1, &id);
}

