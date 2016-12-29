#include <libgen.h>
#include <string.h>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#include "efsw/efsw.hpp"

#include "watcher.hpp"

static efsw::FileWatcher* fileWatcher;
static std::map<std::string, std::vector<void(*)(const std::string&)>> callbacks;

class UpdateListener : public efsw::FileWatchListener
{
public:
    UpdateListener() {}

    void handleFileAction( efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename = "" )
    {
        if (!callbacks[filename].empty()) {
            for (auto& clb : callbacks[filename]) {
                clb(filename);
            }
        }
    }
};

static UpdateListener* listener;

void watcher_initialize(void)
{
    fileWatcher = new efsw::FileWatcher();
    listener = new UpdateListener();
    fileWatcher->watch();
}

void watcher_add_file(const std::string& filename, void(*clb)(const std::string& filename))
{
    if (!fileWatcher) return;

    auto& vec = callbacks[filename];
    if (std::find(vec.begin(), vec.end(), clb) != vec.end()) {
        // we assumed it's already watched
        return;
    }

    char* ddir = strdup(filename.c_str());
    char* dir = dirname(ddir);
    fileWatcher->addWatch(dir, listener, false);
    callbacks[filename].push_back(clb);
    free(ddir);
}

