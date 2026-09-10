#include <algorithm>
#include <glob.h>
#include <regex>
#include <tuple>
#ifdef USE_GDAL
#include <gdal.h>
#include <gdal_priv.h>
#endif

#include <alphanum.hpp>
#include <doctest.h>

#include "collection_expression.hpp"
#include "fs.hpp"
#include "strutils.hpp"

#define SORT(x) \
    std::sort(x.begin(), x.end(), doj::alphanum_less<std::string>());

static std::vector<std::string> expand_zip(const std::string& path)
{
    std::vector<std::string> subfiles;
#ifdef USE_GDAL
    std::string zippath = path;
    if (!startswith(zippath, "/vsizip/")) {
        zippath = "/vsizip/" + zippath;
    }
    if (!endswith(zippath, "/")) {
        zippath = zippath + "/";
    }
    char** listing = VSIReadDirRecursive(zippath.c_str());
    if (listing) {
        for (int i = 0; listing[i]; i++) {
            if (endswith(listing[i], "/")) {
                continue;
            }
            subfiles.push_back(zippath + listing[i]);
        }
        CSLDestroy(listing);
    } else {
        fprintf(stderr, "looks like the zip '%s' is empty\n", zippath.c_str());
    }

    SORT(subfiles);
#else
    fprintf(stderr, "reading from zip require GDAL support\n");
    subfiles.push_back(path);
#endif
    return subfiles;
}

static std::vector<std::string> expand_s3(const std::string& path)
{
    std::vector<std::string> subfiles;
#ifdef USE_GDAL
    char** listing = VSIReadDirRecursive(path.c_str());
    if (listing) {
        for (int i = 0; listing[i]; i++) {
            if (endswith(listing[i], "/")) {
                continue;
            }

            std::string fullname = path + listing[i];
            subfiles.push_back(fullname);
        }
        CSLDestroy(listing);
    } else {
        fprintf(stderr, "looks like the S3 uri '%s' is empty\n", path.c_str());
    }

    SORT(subfiles);
#else
    fprintf(stderr, "listings on S3 require GDAL support\n");
    subfiles.push_back(path);
#endif
    return subfiles;
}

static std::vector<std::string> expand_path(const std::string& path)
{
    if (endswith(path, ".zip")) {
        return expand_zip(path);
    }
    if (startswith(path, "/vsis3/") && endswith(path, "/")) {
        return expand_s3(path);
    }
    return { path };
}

static inline void convert_for_gdal(std::string& path)
{
#ifdef USE_GDAL
    if (startswith(path, "s3://")) {
        path = std::regex_replace(path, std::regex("s3://"), "/vsis3/");
    }
    if (startswith(path, "https://")) {
        path = std::regex_replace(path, std::regex("https://"), "/vsicurl/https://");
    }
    if (startswith(path, "http://")) {
        path = std::regex_replace(path, std::regex("http://"), "/vsicurl/http://");
    }
#endif
}

static std::vector<std::string> do_glob(const std::string& expr)
{
    glob_t res;
    ::glob(expr.c_str(), GLOB_TILDE | GLOB_NOSORT | GLOB_BRACE, nullptr, &res);
    std::vector<std::string> results;
    for (unsigned int j = 0; j < res.gl_pathc; j++) {
        results.push_back(res.gl_pathv[j]);
    }
    globfree(&res);
    SORT(results);
    return results;
}

static std::vector<std::string> try_split(const std::string& expr)
{
    static std::regex sep { R"(::)" };
    std::vector<std::string> results;
    split(expr, std::back_inserter(results), sep);
    return results;
}

static void list_directory(const std::string& path, std::vector<std::string>& directories, std::vector<std::string>& files)
{
    std::vector<std::string> results;
    auto it = fs::directory_iterator(path,
        fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied);
    for (const auto& entry : it) {
        auto& file = entry.path();

        if (file.filename().string()[0] == '.') {
            continue;
        }

        if (entry.is_directory()) {
            directories.push_back(file.u8string());
        } else {
            files.push_back(file.u8string());
        }
    }
}

static std::vector<std::string> collect_directory(const std::string& path)
{
    std::vector<std::string> results;

    std::vector<std::string> directories, files;
    list_directory(path, directories, files);

    std::vector<std::pair<std::string, bool>> sorted;
    for (auto file : files) {
        sorted.push_back(std::make_pair(file, false));
    }
    for (auto dir : directories) {
        sorted.push_back(std::make_pair(dir, true));
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return doj::alphanum_comp(a.first, b.first) < 0; });

    for (auto& info : sorted) {
        const std::string& path = info.first;
        bool is_dir = info.second;
        if (is_dir) {
            auto subfiles = collect_directory(path);
            for (auto f : subfiles) {
                results.push_back(f);
            }
        } else {
            auto expanded = expand_path(path);
            std::copy(expanded.cbegin(), expanded.cend(), std::back_inserter(results));
        }
    }
    return results;
}

std::vector<fs::path> buildFilenamesFromExpression(const std::string& expr)
{
    std::vector<std::string> filenames;

    for (auto subexpr : try_split(expr)) {
        auto globres = do_glob(subexpr);

        if (globres.size() == 0) {
            // vpv /vsicurl/https://download.osgeo.org/gdal/data/gtiff/small_world.tif
            // it's not a file, so it won't be in the globres
            convert_for_gdal(subexpr);
            auto expanded = expand_path(subexpr);
            std::copy(expanded.cbegin(), expanded.cend(), std::back_inserter(filenames));
        } else {
            for (auto file : globres) {
                if (fs::is_directory(file)) {
                    auto indir = collect_directory(file);
                    std::copy(indir.cbegin(), indir.cend(), std::back_inserter(filenames));
                } else {
                    auto expanded = expand_path(file);
                    std::copy(expanded.cbegin(), expanded.cend(), std::back_inserter(filenames));
                }
            }
        }
    }

    if (filenames.empty() && expr == "-") {
        filenames.push_back("-");
    }

    std::vector<fs::path> paths;
    std::transform(filenames.cbegin(), filenames.cend(),
        std::back_inserter(paths),
        [](const auto& filename) { return fs::path(filename); });

    return paths;
}

std::vector<fs::path> applySortfilt(const std::vector<fs::path>& paths, const std::string& regex)
{
    auto re = std::regex(regex);
    std::vector<std::pair<fs::path, std::vector<std::string>>> matches;
    bool has_subparts = false;

    for (const auto& p : paths) {
        if (!p.has_filename())
            continue;

        auto& filename = p.native();
        std::smatch smatch;
        if (std::regex_match(filename, smatch, re)) {
            std::vector<std::string> parts;
            parts.push_back(smatch[0]);
            if (smatch.size() > 1) {
                for (int i = 1; i < smatch.size(); i++) {
                    parts.push_back(smatch[i]);
                }
                has_subparts = true;
            }
            matches.push_back(std::make_pair(p, parts));
        }
    }

    if (has_subparts) {
        // we could implement a reordering of the matches to be more versatile
        // but I'm not sure which user interface would work well
        std::sort(matches.begin(), matches.end(), [](auto& A, auto& B) {
            auto& mA = A.second;
            auto& mB = B.second;
            assert(mA.size() == mB.size());
            for (int i = 1; i < mA.size(); i++) {
                auto& a = mA[i];
                auto& b = mB[i];
                int c = doj::alphanum_comp(a, b);
                if (c != 0) {
                    return c < 0;
                }
            }
            auto& fA = A.second[1];
            auto& fB = B.second[1];
            return doj::alphanum_comp(A.first.string(), B.first.string()) < 0;
        });
    }

    std::vector<fs::path> newpaths;
    for (const auto& p : matches) {
        newpaths.push_back(p.first);
    }

    return newpaths;
}

TEST_CASE("buildFilenamesFromExpression")
{
    SUBCASE("-")
    {
        auto v = buildFilenamesFromExpression("-");
        CHECK(v.size() == 1);
        if (v.size() > 0)
            CHECK(v[0] == "-");
    }

    SUBCASE("src (flat)")
    {
        auto v = buildFilenamesFromExpression("../src");
        auto it = std::find(v.begin(), v.end(), std::string("../src/fuzzy-finder/Cargo.lock"));
        if (it != v.end())
            v.erase(it);
        CHECK(v.size() == 77);
        if (v.size() > 0)
            CHECK(v[0] == "../src/Colormap.cpp");
        if (v.size() > 1)
            CHECK(v[1] == "../src/Colormap.hpp");
    }

    SUBCASE("src/*.cpp (glob)")
    {
        auto v = buildFilenamesFromExpression("../src/*.cpp");
        CHECK(v.size() == 34);
        if (v.size() > 0)
            CHECK(v[0] == "../src/Colormap.cpp");
        if (v.size() > 1)
            CHECK(v[1] == "../src/DisplayArea.cpp");
    }

    SUBCASE("external (recursive)")
    {
        auto v = buildFilenamesFromExpression("../external");
        CHECK(v.size() >= 400);
        if (v.size() > 0)
            CHECK(v[0] == "../external/dirent/dirent.h");
        if (v.size() > 1)
            CHECK(v[1] == "../external/doctest/doctest.h");
    }

    SUBCASE("src::external (::)")
    {
        auto v1 = buildFilenamesFromExpression("../src");
        auto v2 = buildFilenamesFromExpression("../external");
        auto v = buildFilenamesFromExpression("../src::../external");
        CHECK(v.size() == v1.size() + v2.size());
        if (v.size() > 0)
            CHECK(v[0] == v1[0]);
        if (v.size() > 1)
            CHECK(v[1] == v1[1]);
        if (v.size() > v1.size())
            CHECK(v[v1.size()] == v2[0]);
    }
}
