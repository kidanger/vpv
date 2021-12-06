#pragma once

#include <string>
#include <vector>

#include "fs.hpp"

class ImageCollection;

std::vector<fs::path> buildFilenamesFromExpression(const std::string& expr);
