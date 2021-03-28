#pragma once

#include <regex>

template<typename Out>
static void split(const std::string& s, Out result) {
    // https://stackoverflow.com/a/45204031
    std::regex regex{R"(::)"};
    std::sregex_token_iterator it{s.begin(), s.end(), regex, -1};
    std::vector<std::string> items{it, {}};
    for (auto& i : items) {
        *(result++) = i;
    }
}

bool endswith(const std::string& fullString, const std::string& ending);

