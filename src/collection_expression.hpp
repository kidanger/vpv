#pragma once

#include <memory>
#include <string>
#include <vector>

class ImageCollection;

std::vector<std::string> buildFilenamesFromExpression(const std::string& expr);

void recursive_collect(std::vector<std::string>& filenames, std::string glob);
