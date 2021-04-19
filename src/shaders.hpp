#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Shader.hpp"

extern std::vector<std::shared_ptr<Shader::Program>> gShaders;

std::shared_ptr<Shader::Program> createShader(const std::string& tonemap, const std::string& name = "<unnamed>");
bool loadShader(const std::string& name, const std::string& tonemap);
std::shared_ptr<Shader::Program> getShader(const std::string& name);
