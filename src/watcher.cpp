#include <climits>
#include <libgen.h>
#include <functional>
#include <string>
#include <string.h>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <set>
#include <mutex>

#include "efsw/efsw.hpp"

#include "watcher.hpp"

static efsw::FileWatcher* fileWatcher;
static std::map<std::string, std::vector<std::pair<std::string, std::function<void(const std::string&)>>>> callbacks;
static std::set<std::string> events;
static std::mutex eventsLock;

class UpdateListener : public efsw::FileWatchListener
{
public:
    UpdateListener() {}

    void handleFileAction( efsw::WatchID watchid, const std::string& dir,
                           const std::string& filename, efsw::Action action,
                           std::string oldFilename = "" )
    {
        std::string fullpath = dir + (dir[dir.length()-1] != '/' ? "/" : "") + filename;
        eventsLock.lock();
        events.insert(fullpath);
        eventsLock.unlock();
    }
};

static UpdateListener* listener;

void watcher_initialize(void)
{
    fileWatcher = new efsw::FileWatcher();
    listener = new UpdateListener();
    fileWatcher->watch();
}

void watcher_add_file(const std::string& filename, std::function<void(const std::string&)> clb)
{
    if (!fileWatcher) return;

    char fullpath[PATH_MAX+1];
    realpath(filename.c_str(), fullpath);
    char dir[PATH_MAX+1];
    strcpy(dir, fullpath);
    char* d = dirname(dir);
    if (d != dir)
        strcpy(dir, d);
    fileWatcher->addWatch(dir, listener, false);
    callbacks[fullpath].push_back(std::make_pair(filename, clb));
}

void watcher_check(void)
{
    eventsLock.lock();
    auto eventsCopy = events;
    events.clear();
    eventsLock.unlock();

    for (auto& fullpath : eventsCopy) {
        for (auto& clb : callbacks[fullpath]) {
            clb.second(fullpath);
        }
    }
}

