#pragma once

#include <map>
#include <string>

#include <GL/gl3w.h>

class Shader {
private:
    std::string ID;
    std::string name;

    std::string codeVertex;
    std::string codeFragment;

    GLuint program;

    std::map<std::string, GLint> _uniform_locations;

public:
    Shader(const std::string &vertex, const std::string &fragment, const std::string &name = "default");
    ~Shader();

    // Since a Shader relies on an OpenGL `program`, it should not be copied.
    // Otherwise, when one Shader (copied) is deleted, the other Shader will have its
    // program deleted as well.
    Shader(const Shader &other) = delete;
    Shader& operator=(const Shader &other) = delete;

    bool compile();
    void bind();

    void setParameter(const std::string& name, float a, float b, float c);

    GLuint getProgram() const;
    const std::string& getName() const;
};

