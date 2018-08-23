#pragma once

#include <memory>
#include <string>

struct Image;

namespace ImageCache {

    bool has(const std::string& key);

    std::shared_ptr<Image> get(const std::string& key);

    void store(const std::string& key, std::shared_ptr<Image> image);

    void flush();

    namespace Error {

        bool has(const std::string& key);

        std::string get(const std::string& key);

        void store(const std::string& key, const std::string& message);

    }
};

