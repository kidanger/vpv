#include <regex>
#include <glob.h>
#ifdef USE_GDAL
#include <gdal.h>
#include <gdal_priv.h>
#endif

#include <alphanum.hpp>

#include "strutils.hpp"
#include "collection_expression.hpp"
#include "fs.hpp"

static void try_to_read_a_zip(const std::string& path, std::vector<std::string>& filenames)
{
#ifdef USE_GDAL
    std::string zippath = "/vsizip/" + path + "/";
    char** listing = VSIReadDirRecursive(zippath.c_str());
    std::vector<std::string> subfiles;
    if (listing) {
        for (int i = 0; listing[i]; i++) {
            subfiles.push_back(zippath + listing[i]);
        }
        CSLDestroy(listing);
    } else {
        fprintf(stderr, "looks like the zip '%s' is empty\n", path.c_str());
    }

    std::sort(subfiles.begin(), subfiles.end(), doj::alphanum_less<std::string>());
    std::copy(subfiles.cbegin(), subfiles.cend(), std::back_inserter(filenames));
#else
    fprintf(stderr, "reading from zip require GDAL support\n");
#endif
}

void recursive_collect(std::vector<std::string>& filenames, std::string glob)
{
    // TODO: unit test all that
    std::vector<std::string> collected;

    glob_t res;
    ::glob(glob.c_str(), GLOB_TILDE | GLOB_NOSORT | GLOB_BRACE, nullptr, &res);
    for(unsigned int j = 0; j < res.gl_pathc; j++) {
        std::string file(res.gl_pathv[j]);
        if (fs::is_directory(file)) {
            std::string dirglob = file + (file[file.length()-1] != '/' ? "/*" : "*");
            recursive_collect(collected, dirglob);
        } else {
            collected.push_back(file);
        }
    }
    globfree(&res);

    if (collected.empty()) {
        std::vector<std::string> substr;
        split(glob, std::back_inserter(substr));
        if (substr.size() >= 2) {
            for (const std::string& s : substr) {
                recursive_collect(filenames, s);
            }
        } else {
            // if it's not collected nor splittable, it might be some virtual file
#ifdef USE_GDAL
            if (!strncmp(glob.c_str(), "s3://", 5)) {
                glob = std::regex_replace(glob, std::regex("s3://"), "/vsis3/");
            }
            if (!strncmp(glob.c_str(), "https://", 8)) {
                glob = std::regex_replace(glob, std::regex("https://"), "/vsicurl/https://");
            }
            if (!strncmp(glob.c_str(), "http://", 7)) {
                glob = std::regex_replace(glob, std::regex("http://"), "/vsicurl/http://");
            }
#endif
            if (endswith(glob, ".zip")) {
                try_to_read_a_zip(glob, filenames);
            } else {
                filenames.push_back(glob);
            }
        }

    } else {
        std::sort(collected.begin(), collected.end(), doj::alphanum_less<std::string>());
        for (const auto& str : collected) {
            if (endswith(str, ".zip")) {
                try_to_read_a_zip(str, filenames);
            } else {
                filenames.push_back(str);
            }
        }
    }
}

std::vector<std::string> buildFilenamesFromExpression(const std::string& expr)
{
    std::vector<std::string> filenames;
    recursive_collect(filenames, expr);

    if (filenames.empty() && expr == "-") {
        filenames.push_back("-");
    }

    return filenames;
}

