#pragma once

#include <string>

namespace config {

    void load();

    float get_float(const std::string& name);
    bool get_bool(const std::string& name);
    std::string get_string(const std::string& name);

}

