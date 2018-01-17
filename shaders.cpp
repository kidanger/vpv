#include <string>
#include <map>
#include <array>

#include "Shader.hpp"
#include "shaders.hpp"
#include "globals.hpp"

#define S(...) #__VA_ARGS__

static std::string defaultVertex = S(
    void main()
    {
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
        gl_FrontColor = gl_Color;
    }
);

bool loadShader(const std::string& name, const std::string& mainFragment)
{
    Shader* shader = new Shader;
    shader->name = name;
    std::copy(mainFragment.begin(), mainFragment.end()+1, shader->codeFragment);
    std::copy(defaultVertex.begin(), defaultVertex.end()+1, shader->codeVertex);
    if (shader->compile() || 1) {
        gShaders.push_back(shader);
        return true;
    }
    return false;
}

Shader* getShader(const std::string& name)
{
    for (auto s : gShaders) {
        if (s->name == name)
            return s;
    }
    return nullptr;
}

