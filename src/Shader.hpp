#pragma once

#include <string>

struct Shader {
    std::string ID;
    std::string name;

    std::string codeVertex;
    std::string codeFragment;

    Shader(const std::string &vertex, const std::string &fragment);
    ~Shader();

    bool compile();
    void bind();

    void setParameter(const std::string& name, float a, float b, float c);

    unsigned int program;
private:
};

