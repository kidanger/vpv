#pragma once

#include <string>
#include <map>

void loadDefaultShaders();
bool loadShader(const std::string& name, const std::string& tonemap);
Shader* getShader(const std::string& name);

