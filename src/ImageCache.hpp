#pragma once

#include <string>

struct Image;

namespace ImageCache {

    bool has(const std::string& key);

    Image* get(const std::string& key);

    void store(const std::string& key, Image* image);

    void flush();

};

