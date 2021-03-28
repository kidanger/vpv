#include <algorithm>
#include <string>
#include <map>
#include <array>

#include "Shader.hpp"
#include "shaders.hpp"
#include "globals.hpp"

std::vector<Shader::Program*> gShaders;

#define S(...) #__VA_ARGS__

static const std::string defaultVertex = S(
    uniform mat4 v_transform;
    layout(location=1) in vec2 v_position;
    layout(location=2) in vec2 v_texcoord;
    layout(location=3) in vec4 v_color;
    out vec2 f_texcoord;
    out vec4 f_color;
    void main()
    {
        f_texcoord = v_texcoord;
        f_color = v_color;
        gl_Position = v_transform * vec4(v_position.xy,0,1);
    }
);

Shader::Program* createShader(const std::string& mainFragment, const std::string &name)
{
    return new Shader::Program({
                    Shader::Vertex(defaultVertex),
                    Shader::Fragment(mainFragment)
               }, name);
}

bool loadShader(const std::string& name, const std::string& mainFragment)
{
    Shader::Program* shader = createShader(mainFragment, name);
    gShaders.push_back(shader);
    std::sort(gShaders.begin(), gShaders.end(),
              [](const Shader::Program* lhs, const Shader::Program* rhs) {
                  return lhs->getName() < rhs->getName();
              });
    return true;
}

Shader::Program* getShader(const std::string& name)
{
    for (auto s : gShaders) {
        if (s->getName() == name)
            return s;
    }
    return nullptr;
}

