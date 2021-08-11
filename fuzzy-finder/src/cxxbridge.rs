use crate::{FuzzyPathFinder, FuzzyPathFinderBuilder, FuzzyStringMatcher, Match, IndexedMatch};

#[cxx::bridge]
mod ffi {
    #[namespace = "murky"]
    extern "Rust" {
        type FuzzyStringMatcher;
        type FuzzyPathFinder;
        type Match;
        type IndexedMatch;

        fn to_str(self: &Match) -> &str;
        fn score(self: &Match) -> i64;
        fn indices(self: &Match) -> &Vec<usize>;

        fn to_str(self: &IndexedMatch) -> &str;
        fn score(self: &IndexedMatch) -> i64;
        fn indices(self: &IndexedMatch) -> &Vec<usize>;
        fn index(self: &IndexedMatch) -> usize;

        fn fuzzy_string_matcher() -> Box<FuzzyStringMatcher>;
        fn matches(self: &FuzzyStringMatcher, values: &[&str], pattern: &str) -> Vec<IndexedMatch>;

        fn fuzzy_path_finder(base: &str) -> Box<FuzzyPathFinder>;
        fn matches_skip_and_take(
            self: &mut FuzzyPathFinder,
            pattern: &str,
            skip: usize,
            take: usize,
        ) -> Vec<Match>;
    }
}

pub fn fuzzy_string_matcher() -> Box<FuzzyStringMatcher> {
    Box::new(FuzzyStringMatcher::default())
}

fn fuzzy_path_finder(base: &str) -> Box<FuzzyPathFinder> {
    Box::new(FuzzyPathFinderBuilder::from_path(base).build())
}
