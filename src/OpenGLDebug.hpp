#pragma once

#include <cstdio>
#include <string>

#include <GL/gl3w.h>

#define GLDEBUG() \
{ \
    GLenum e; \
    while((e = glGetError()) != GL_NO_ERROR) \
    { \
        std::printf("%s:%s:%d\n", getGLError(e), __FILE__, __LINE__); \
    } \
}

const char *getGLError(GLenum error);
