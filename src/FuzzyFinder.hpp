#pragma once

#include <string>
#include <vector>
#include <memory>

#include "murky.hpp"

class FuzzyFinder {

    std::vector<std::string> vec;
    rust::Box<murky::FuzzyStringMatcher> matcher;

public:

    FuzzyFinder(const std::vector<std::string>& vec) : vec(vec), matcher(murky::fuzzy_string_matcher()) {
    }

    rust::Vec<murky::IndexedMatch> matches(const std::string& pattern) {
        rust::Vec<rust::Str> vals;
        std::copy(vec.begin(), vec.end(), std::back_inserter(vals));
        auto values = rust::Slice<const rust::Str>(vals.data(), vals.size());
        auto matches = matcher->matches(values, pattern);
        return matches;
    }

};

class FuzzyFinderGUI {

    std::shared_ptr<FuzzyFinder> finder;
    struct Sequence* currentSequence;
    rust::Vec<murky::IndexedMatch> matches;
    char buf[1024] = {0};
    int current;
    bool focus;

public:

    void open();
    void display(struct Sequence&);

};

