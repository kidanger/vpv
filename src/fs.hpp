#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (defined(__cplusplus) && __cplusplus >= 201703L)) && defined(__has_include)
#if __has_include(<filesystem>) && (!defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs {
    using namespace std::filesystem;
    using ifstream = std::ifstream;
    using ofstream = std::ofstream;
    using fstream = std::fstream;
}
#endif
#endif
#ifndef GHC_USE_STD_FS
#include <ghc/filesystem.hpp>
namespace fs {
    using namespace ghc::filesystem;
    using ifstream = ghc::filesystem::ifstream;
    using ofstream = ghc::filesystem::ofstream;
    using fstream = ghc::filesystem::fstream;
}
#endif