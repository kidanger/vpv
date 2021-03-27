#pragma once

#include <string>
#include <map>

class Shader;

Shader* createShader(const std::string& tonemap, const std::string &name = "default");
bool loadShader(const std::string& name, const std::string& tonemap);
Shader* getShader(const std::string& name);

