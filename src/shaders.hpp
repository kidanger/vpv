#pragma once

#include <string>
#include <vector>

#include "Shader.hpp"

extern std::vector<Shader::Program *> gShaders;

Shader::Program* createShader(const std::string& tonemap, const std::string &name = "<unnamed>");
bool loadShader(const std::string& name, const std::string& tonemap);
Shader::Program* getShader(const std::string& name);

