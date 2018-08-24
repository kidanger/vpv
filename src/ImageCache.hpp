#pragma once

#include <string>
#include <memory>

struct Image;

namespace ImageCache {

    bool has(const std::string& key);

    std::shared_ptr<Image> get(const std::string& key);

    void store(const std::string& key, std::shared_ptr<Image> image);

    bool remove(const std::string& key);
    bool remove_rec(const std::string& key);

    bool isFull();

    void flush();

    namespace Error {

        bool has(const std::string& key);

        std::string get(const std::string& key);

        void store(const std::string& key, const std::string& message);

        bool remove(const std::string& key);

    }
}

