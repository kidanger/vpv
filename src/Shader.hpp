#pragma once

#include <string>

#include <GL/gl3w.h>

class Shader {
private:
    std::string ID;
    std::string name;

    std::string codeVertex;
    std::string codeFragment;

    GLuint program;

public:
    Shader(const std::string &vertex, const std::string &fragment, const std::string &name = "default");
    ~Shader();

    bool compile();
    void bind();

    void setParameter(const std::string& name, float a, float b, float c);

    GLuint getProgram() const;
    const std::string& getName() const;
};

