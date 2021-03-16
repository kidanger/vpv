#include <functional>
#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <set>
#include <mutex>
#include <system_error>

#include "efsw/efsw.hpp"

#include "fs.hpp"
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
#ifdef WINDOWS
        std::string fullpath = dir + ((dir[dir.length()-1] != '/' && dir[dir.length()-1] != '\\') ? "\\" : "") + filename;
#else
        std::string fullpath = dir + (dir[dir.length()-1] != '/' ? "/" : "") + filename;
#endif
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
    std::error_code ec;

    if (!fileWatcher) return;

    const fs::path fullpath = fs::absolute(fs::path(filename), ec);
    if (ec) {
        return;
    }

    fileWatcher->addWatch(fullpath.parent_path().u8string(), listener, false);
    callbacks[fullpath.u8string()].push_back(std::make_pair(filename, clb));
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

