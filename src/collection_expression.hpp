#pragma once

#include <string>
#include <vector>

#include "fs.hpp"

class ImageCollection;

std::vector<fs::path> buildFilenamesFromExpression(const std::string& expr);

std::vector<fs::path> applySortfilt(const std::vector<fs::path>& paths, const std::string& regex);
