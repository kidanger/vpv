#pragma once

#include <string>
#include "kaguya/kaguya.hpp"

namespace config {

    void load();

    float get_float(const std::string& name);
    bool get_bool(const std::string& name);
    int get_int(const std::string& name);
    std::string get_string(const std::string& name);
    void load_shaders();

    kaguya::State& get_lua();

}

