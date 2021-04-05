#include <string>
#include <vector>
#include <utility>

#include <GL/gl3w.h>
#include <imgui.h>

#include "globals.hpp"

#include "Shader.hpp"
#include "OpenGLDebug.hpp"

// Helper function to get an item from a map
// or insert an element generated thanks to a functor
// and return the value.
template <class M, class Key, class F>
typename M::mapped_type &
get_or_insert_with(M &map, Key const& key, F functor)
{
    using V=typename M::mapped_type;
    std::pair<typename M::iterator, bool> r = map.insert(typename M::value_type(key, V()));
    V &value = r.first->second;
    if (r.second) {
       functor(value);
    }
    return value;
}

namespace Shader {

constexpr auto GLSL_HEADER = "#version 330 core\n#ifdef GL_ES\nprecision mediump float;\n#endif\n";

Shader::Shader(Type type, const std::string &code) :
    _shader_id(glCreateShader(static_cast<GLenum>(type)))
{
    static int id = 0;
    id++;
    ID = "Shader " + std::to_string(id);

    if (_shader_id > 0) {
        std::string header_and_code = GLSL_HEADER + code;
        const char *codePtr = header_and_code.c_str();
        glShaderSource(_shader_id, 1, &codePtr, nullptr);
    }
}

Shader::~Shader()
{
    glDeleteShader(_shader_id);
}

Shader::Shader(Shader&& other) noexcept {
    *this = std::move(other);
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    std::swap(_shader_id, other._shader_id);
    ID = std::move(other.ID);
    other._shader_id = 0;
    return *this;
}

bool Shader::compile()
{
    GLint result;
    GLsizei infoLogLength;

    GLDEBUG();
    glCompileShader(_shader_id);
    GLDEBUG();

    glGetShaderiv(_shader_id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        glGetShaderiv(_shader_id, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            std::vector<char> msg(infoLogLength+1);
            glGetShaderInfoLog(_shader_id, infoLogLength, nullptr, &msg[0]);
            fprintf(stderr, "vertex: %s\n", &msg[0]);
            return false;
        }
    }

    GLDEBUG();
    return true;
}

Program::Program(std::initializer_list<Shader> shaders, std::string name) :
    _program_id(glCreateProgram()), _uniform_locations(), _name(std::move(name))
{
    GLDEBUG();
    for (const auto &shader : shaders) {
        glAttachShader(_program_id, shader());
    }

    glLinkProgram(_program_id);

    for (const auto &shader : shaders) {
        glDetachShader(_program_id, shader());
    }
    GLDEBUG();
}

Program::Program(Program&& other) noexcept {
    *this = std::move(other);
}

Program& Program::operator=(Program&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    std::swap(_program_id, other._program_id);
    _uniform_locations = std::move(other._uniform_locations);
    _name = std::move(other._name);
    other._program_id = 0;
    return *this;
}

Program::~Program()
{
    if (_program_id > 0) {
        glDeleteProgram(_program_id);
    }
}

void Program::bind()
{
    GLDEBUG();
    glUseProgram(_program_id);
    GLint loc = glGetUniformLocation(_program_id, "tex");
    if (loc >= 0) {
        glUniform1i(loc, 0);
    }
    GLDEBUG();
}

void Program::setParameter(const std::string& name, float a, float b, float c)
{
    GLDEBUG();
    auto insert_uniform_location = [this, &name](GLint &uniform_location){
        uniform_location = glGetUniformLocation(_program_id, name.c_str());
    };
    GLint uniform_location = get_or_insert_with(_uniform_locations, name, insert_uniform_location);
    if (uniform_location >= 0) {
        glUniform3f(uniform_location, a, b, c);
    }
    GLDEBUG();
}

GLuint Program::getProgramID() const
{
    return _program_id;
}

const std::string& Program::getName() const
{
    return _name;
}

}
