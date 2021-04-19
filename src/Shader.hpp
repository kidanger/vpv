#pragma once

#include <initializer_list>
#include <map>
#include <string>

#include <GL/gl3w.h>

// To create a shader use the following types:
//
//     Shader::Vertex vertex(code);
//     Shader::Fragment fragment(code);
//     ...
//
// To create a program:
//
//     Shader::Program program({
//         Shader::Vertex(code),
//         Shader::Fragment(code)
//     });
namespace Shader {
enum class Type : GLenum {
    Compute = GL_COMPUTE_SHADER,
    Vertex = GL_VERTEX_SHADER,
    TessControl = GL_TESS_CONTROL_SHADER,
    TessEvaluation = GL_TESS_EVALUATION_SHADER,
    Geometry = GL_GEOMETRY_SHADER,
    Fragment = GL_FRAGMENT_SHADER
};

class Program;

class Shader {
private:
    std::string ID;

    GLuint _shader_id;

    GLuint operator()() const
    {
        return _shader_id;
    }

public:
    Shader(Type type, const std::string& code);
    ~Shader();

    // Since a Shader relies on an OpenGL shader resource, it should not be copied.
    // Otherwise, when one Shader (copied) is deleted, the other Shader will have its
    // shader resource deleted as well.
    Shader(const Shader& other) = delete;
    Shader& operator=(const Shader& other) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool compile();

    friend class Program;
};

template <Type type>
class ShaderType : public Shader {
public:
    explicit ShaderType(const std::string& code)
        : Shader(type, code)
    {
        compile();
    }
};

using Vertex = ShaderType<Type::Vertex>;
using Fragment = ShaderType<Type::Fragment>;

class Program {
public:
    Program(std::initializer_list<Shader> shaders, std::string name = "<unnamed>");
    ~Program();

    // Since a ShaderProgram relies on an OpenGL `program`, it should not be copied.
    // Otherwise, when one ShaderProgram (copied) is deleted, the other ShaderProgram will have its
    // program deleted as well.
    Program(const Program& other) = delete;
    Program& operator=(const Program& other) = delete;

    Program(Program&& other) noexcept;
    Program& operator=(Program&& other) noexcept;

    void bind();
    void setParameter(const std::string& name, float a, float b, float c);
    GLuint getProgramID() const;
    const std::string& getName() const;

private:
    GLuint _program_id;
    std::map<std::string, GLint> _uniform_locations;
    std::string _name;
};

}
