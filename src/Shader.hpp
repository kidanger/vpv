#pragma once

#include <string>

class Shader {
private:
    std::string ID;
    std::string name;

    std::string codeVertex;
    std::string codeFragment;

    unsigned int program;

public:
    Shader(const std::string &vertex, const std::string &fragment, const std::string &name = "default");
    ~Shader();

    bool compile();
    void bind();

    void setParameter(const std::string& name, float a, float b, float c);

    unsigned int getProgram() const;
    const std::string& getName() const;
};

