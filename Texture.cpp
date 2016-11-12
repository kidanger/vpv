#include <SFML/OpenGL.hpp>

#include "Texture.hpp"

void Texture::create(int w, int h, unsigned int type, unsigned int format)
{
    if (id == -1) {
        glGenTextures(1, &id);
    }

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, format, type, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glBindTexture(GL_TEXTURE_2D, 0);

    size.x = w;
    size.y = h;
    type = type;
    format = format;
}

Texture::~Texture() {
    glDeleteTextures(1, &id);
}

