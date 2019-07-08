#pragma once

#include <string>
#include <map>

struct Shader;

Shader* createShader(const std::string& tonemap);
bool loadShader(const std::string& name, const std::string& tonemap);
Shader* getShader(const std::string& name);

