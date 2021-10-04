#pragma once

#include <regex>

template <typename Out>
static void split(const std::string& s, Out result, const std::regex& re)
{
    // https://stackoverflow.com/a/45204031
    std::sregex_token_iterator it { s.begin(), s.end(), re, -1 };
    std::vector<std::string> items { it, {} };
    for (auto& i : items) {
        *(result++) = i;
    }
}

bool startswith(const std::string& fullString, const std::string& start);
bool endswith(const std::string& fullString, const std::string& ending);
