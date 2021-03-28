#pragma once

#include <vector>
#include <string>
#include <memory>

class ImageCollection;

std::vector<std::string> buildFilenamesFromExpression(const std::string& expr);

void recursive_collect(std::vector<std::string>& filenames, std::string glob);

