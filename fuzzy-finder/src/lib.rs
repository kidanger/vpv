use std::collections::BinaryHeap;
use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock, RwLockReadGuard};
use std::thread::{self};

use fuzzy_matcher::{skim::SkimMatcherV2, FuzzyMatcher};
use ignore::{DirEntry, WalkBuilder, WalkState};
use lru::LruCache;

mod cxxbridge;

#[derive(Default)]
pub struct FuzzyStringMatcher(SkimMatcherV2);

#[derive(Default, Clone, Debug, PartialEq, Eq)]
pub struct Match {
    string: String,
    score: i64,
    indices: Vec<usize>,
}

#[derive(Default, Clone, Debug, PartialEq, Eq)]
pub struct IndexedMatch {
    match_: Match,
    index: usize,
}

impl PartialOrd for Match {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        self.score.partial_cmp(&other.score())
    }
}

impl Ord for Match {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.score.cmp(&other.score())
    }
}

impl Match {
    pub fn to_str(&self) -> &str {
        self.string.as_str()
    }

    pub fn score(&self) -> i64 {
        self.score
    }

    pub fn indices(&self) -> &Vec<usize> {
        &self.indices
    }
}

impl IndexedMatch {
    pub fn to_str(&self) -> &str {
        self.match_.to_str()
    }

    pub fn score(&self) -> i64 {
        self.match_.score()
    }

    pub fn indices(&self) -> &Vec<usize> {
        self.match_.indices()
    }

    pub fn index(&self) -> usize {
        self.index
    }
}

impl FuzzyStringMatcher {
    pub fn matches(&self, values: &[&str], pattern: &str) -> Vec<IndexedMatch> {
        let mut matches = Vec::new();
        values.iter().enumerate().for_each(|(index, value)| {
            if let Some(match_) = self.matching(value, pattern) {
                matches.push(IndexedMatch {
                    match_: match_,
                    index: index,
                })
            }
        });
        matches.sort_unstable_by_key(|a| a.match_.score());

        matches
    }

    fn matching(&self, value: &str, pattern: &str) -> Option<Match> {
        self.0
            .fuzzy_indices(value, pattern)
            .map(|(score, indices)| Match {
                string: value.to_string(),
                score,
                indices,
            })
    }
}

pub struct FuzzyPathFinderBuilder {
    walk_builder: WalkBuilder,
    matcher: Option<FuzzyStringMatcher>,
    blocking: bool,
}

impl FuzzyPathFinderBuilder {
    pub fn from_path(path: impl AsRef<Path>) -> Self {
        Self {
            matcher: None,
            walk_builder: WalkBuilder::new(path),
            blocking: false,
        }
    }

    pub fn from_walk_builder(walk_builder: WalkBuilder) -> Self {
        Self {
            matcher: None,
            walk_builder,
            blocking: false,
        }
    }

    pub fn blocking(mut self, blocking: bool) -> Self {
        self.blocking = blocking;
        self
    }

    pub fn with_fuzzy_string_matcher(mut self, matcher: FuzzyStringMatcher) -> Self {
        self.matcher = Some(matcher);
        self
    }

    pub fn build(self) -> FuzzyPathFinder {
        let paths = Arc::new(RwLock::new(Vec::new()));
        let paths_inner = paths.clone();
        let walker = self.walk_builder.build_parallel();

        // Walks recursively a given directory and stores all the files found.
        let walker_handle = thread::spawn(move || {
            walker.run(move || {
                let paths = paths_inner.clone();
                Box::new(move |entry| {
                    let entry: DirEntry = if let Ok(entry) = entry {
                        entry
                    } else {
                        return WalkState::Continue;
                    };
                    if !entry.file_type().map_or_else(|| false, |t| t.is_file()) {
                        return WalkState::Continue;
                    }
                    let mut paths = if let Ok(paths) = paths.write() {
                        paths
                    } else {
                        return WalkState::Quit;
                    };

                    paths.push(entry.path().into());

                    WalkState::Continue
                })
            });
        });
        if self.blocking {
            let _ = walker_handle.join();
        }

        FuzzyPathFinder {
            string_matcher: self.matcher.unwrap_or_default(),
            cache: LruCache::new(20),
            paths,
        }
    }
}

/// Cache entry that contains all the paths that matched a pattern.
#[derive(Default)]
struct CacheEntry {
    matches: BinaryHeap<Match>,
    /// Number of paths already processed that needs to be skipped
    skip: usize,
}

impl CacheEntry {
    fn update(
        &mut self,
        string_matcher: &FuzzyStringMatcher,
        paths: RwLockReadGuard<Vec<PathBuf>>,
        pattern: &str,
    ) {
        for path in paths.iter().skip(self.skip) {
            self.matches.extend(
                path.to_str()
                    .and_then(|path| string_matcher.matching(path, pattern)),
            );
            self.skip += 1;
        }
    }
}

pub struct FuzzyPathFinder {
    string_matcher: FuzzyStringMatcher,
    cache: LruCache<String, CacheEntry>,
    paths: Arc<RwLock<Vec<PathBuf>>>,
}

impl FuzzyPathFinder {
    pub fn matches_skip_and_take(&mut self, pattern: &str, skip: usize, take: usize) -> Vec<Match> {
        // Note: It's ugly, but `LruCache` does not have an entry API yet (https://github.com/jeromefroe/lru-rs/issues/30)
        let cache_key = pattern.to_string();
        let cache_entry = if let Some(cache_entry) = self.cache.get_mut(&cache_key) {
            cache_entry
        } else {
            let cache_entry = CacheEntry::default();
            self.cache.put(cache_key.clone(), cache_entry);
            self.cache.get_mut(&cache_key).unwrap()
        };

        // TODO: add a thread to update each cache entry?
        cache_entry.update(&self.string_matcher, self.paths.read().unwrap(), pattern);

        cache_entry
            .matches
            .iter()
            .skip(skip)
            .take(take)
            .cloned()
            .collect::<Vec<Match>>()
    }
}

#[cfg(test)]
mod tests {
    use std::fs::File;
    use std::path::PathBuf;
    use std::str::FromStr;

    use tempfile::TempDir;

    use crate::FuzzyPathFinderBuilder;

    fn create_files(tmpdir: &TempDir, filenames: &[&str]) {
        for filename in filenames {
            let _file = File::create(tmpdir.path().join(filename)).expect("Failed to create file");
        }
    }

    fn test(filenames: &[&str], pattern: &str, expected_filenames: &[&str]) {
        let tmpdir = tempfile::tempdir().expect("Failed to create a temporary directory");

        create_files(&tmpdir, filenames);

        let mut finder = FuzzyPathFinderBuilder::from_path(tmpdir.path())
            .blocking(true)
            .build();

        let matches = finder.matches_skip_and_take(pattern, 0, 100);
        let filenames = matches
            .iter()
            .map(|m| {
                let path = PathBuf::from_str(m.to_str()).unwrap();
                assert_eq!(path.parent().unwrap(), tmpdir.path());
                path.file_name().unwrap().to_string_lossy().to_string()
            })
            .collect::<Vec<String>>();
        let filenames_str = filenames.iter().map(|s| s.as_str()).collect::<Vec<&str>>();
        assert_eq!(filenames_str, expected_filenames);
    }

    // Be careful with how you choose the pattern and filenames,
    // otherwise the tests might fail sometimes due to the random name of the tmpdir that can contain letters that match the pattern.

    #[test]
    fn test_fuzzy_path_finder_single_result() {
        test(
            &["foobar.rs", "image.png", "image.jpg"],
            "foobar",
            &["foobar.rs"],
        );
    }

    #[test]
    fn test_fuzzy_path_finder_no_results() {
        test(&["foobar.rs", "image.png", "image.jpg"], "unrelated", &[]);
    }

    #[test]
    fn test_fuzzy_path_finder_multiple_results() {
        test(
            &["foobar.rs", "image.png", "image.jpg"],
            "image.p",
            &["image.png", "image.jpg"],
        );
    }

    #[test]
    fn test_fuzzy_path_finder_multiple_results_ordered() {
        test(
            &[
                "foobar.rs",
                "foobarimage.png",
                "foobarimae.jpg",
                "foobarmage.png",
            ],
            "mage",
            &["foobarmage.png", "foobarimage.png"],
        );
    }
}
