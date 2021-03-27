#include <string>
#include <vector>

#include <GL/gl3w.h>
#include "globals.hpp"
#include "imgui.h"

#include "Shader.hpp"
#include "OpenGLDebug.hpp"

Shader::Shader(const std::string &vertex, const std::string &fragment, const std::string &name) :
    name(name), codeVertex(vertex), codeFragment(fragment)
{
    static int id = 0;
    id++;
    ID = "Shader " + std::to_string(id);
}

Shader::~Shader()
{
    if (program) {
        glDeleteProgram(program);
    }
}

bool Shader::compile()
{
    GLDEBUG();

    GLint result;
    int infoLogLength;

    std::string header = "#version 330 core\n#ifdef GL_ES\nprecision mediump float;\n#endif\n";
    std::string headedCodeVertex = header + codeVertex;
    const char* codeVertexPtr = headedCodeVertex.c_str();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &codeVertexPtr, nullptr);
    glCompileShader(vertexShader);
    GLDEBUG();

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> msg(infoLogLength+1);
            glGetShaderInfoLog(vertexShader, infoLogLength, nullptr, &msg[0]);
            fprintf(stderr, "vertex: %s\n", &msg[0]);
            return false;
        }
    }

    std::string headedCodeFragment = header + codeFragment;
    const char* codeFragmentPtr = headedCodeFragment.c_str();
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &codeFragmentPtr, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> msg(infoLogLength+1);
            glGetShaderInfoLog(fragmentShader, infoLogLength, nullptr, &msg[0]);
            fprintf(stderr, "fragment: %s\n", &msg[0]);
            return false;
        }
    }

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLDEBUG();
    return true;
}

void Shader::bind()
{
    GLDEBUG();
    glUseProgram(program);
    GLuint loc = glGetUniformLocation(program, "tex");
    glUniform1i(loc, 0);
    GLDEBUG();
}

void Shader::setParameter(const std::string& name, float a, float b, float c)
{
    GLDEBUG();
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc >= 0) {
        glUniform3f(loc, a, b, c);
    }
    GLDEBUG();
}

GLuint Shader::getProgram() const
{
    return program;
}

const std::string& Shader::getName() const
{
    return name;
}
