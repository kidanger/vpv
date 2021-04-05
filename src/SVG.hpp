#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include <imgui.h>
#include <nanosvg.h>

struct SVG {
    struct NSVGimage* nsvg;
    std::string filename;
    bool valid;

    ~SVG();

    void draw(ImVec2 basepos, ImVec2 pos, float zoom) const;

    static SVG* get(const std::string& filename);
    static std::shared_ptr<SVG> createFromString(const std::string& str);

    static std::unordered_map<std::string, SVG*> cache;
    static void flushCache();

private:
    SVG();
    void loadFromFile(const std::string& filename);
    void loadFromString(const std::string& str);
};

