#pragma once

#include <memory>
#include <string>
#include <vector>

//#include "fuzzy-finder.hpp"

class FuzzyFinderForSequence {

    rust::Box<fuzzyfinder::FuzzyStringMatcher> matcher;

    std::weak_ptr<class ImageCollection> currentCollection;
    rust::Vec<rust::Str> filenames;

    rust::Vec<fuzzyfinder::IndexedMatch> matches;
    char buf[1024] = { 0 };

    int current;
    bool focus;

    rust::Vec<fuzzyfinder::IndexedMatch> computeMatches(const std::string& pattern)
    {
        auto values = rust::Slice<const rust::Str>(filenames.data(), filenames.size());
        auto matches = matcher->matches(values, pattern);
        return matches;
    }

    void extractFilenames(const std::shared_ptr<class ImageCollection> col);

public:
    FuzzyFinderForSequence()
        : matcher(fuzzyfinder::fuzzy_string_matcher())
    {
    }

    void open();
    void display(struct Sequence&);
};
