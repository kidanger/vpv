
#include "strutils.hpp"

bool startswith(const std::string& fullString, const std::string& start)
{
    return fullString.rfind(start, 0) == 0;
}

bool endswith(const std::string& fullString, const std::string& ending)
{
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

