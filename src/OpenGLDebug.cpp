#include <cstdio>
#include <cstdlib>
#include <string>

#include <GL/gl3w.h>

#include "OpenGLDebug.hpp"

const char* getGLError(GLenum error)
{
#define casereturn(x) \
    case x:           \
        return #x
    switch (error) {
        casereturn(GL_INVALID_ENUM);
        casereturn(GL_INVALID_VALUE);
        casereturn(GL_INVALID_OPERATION);
        casereturn(GL_INVALID_FRAMEBUFFER_OPERATION);
    case GL_OUT_OF_MEMORY:
        std::fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
        std::exit(1);
    default:
    case GL_NO_ERROR:
        return "";
    }
#undef casereturn
}
