#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include <efsw/efsw.hpp>

#include "fs.hpp"
#include "watcher.hpp"

static efsw::FileWatcher* fileWatcher;
static std::map<std::string, std::vector<std::pair<std::string, std::function<void(const std::string&)>>>> callbacks;
static std::set<std::string> events;
static std::mutex eventsLock;

class UpdateListener : public efsw::FileWatchListener {
public:
    UpdateListener() { }

    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
        const std::string& filename, efsw::Action action,
        std::string oldFilename = "") override
    {
#ifdef WINDOWS
        std::string fullpath = dir + ((dir[dir.length() - 1] != '/' && dir[dir.length() - 1] != '\\') ? "\\" : "") + filename;
#else
        std::string fullpath = dir + (dir[dir.length() - 1] != '/' ? "/" : "") + filename;
#endif
        eventsLock.lock();
        events.insert(fullpath);
        eventsLock.unlock();
    }
};

static UpdateListener* listener;

void watcher_initialize()
{
    fileWatcher = new efsw::FileWatcher();
    listener = new UpdateListener();
    fileWatcher->watch();
}

void watcher_add_file(const std::string& filename, const std::function<void(const std::string&)>& clb)
{
    std::error_code ec;

    if (!fileWatcher)
        return;

    const fs::path fullpath = fs::absolute(fs::path(filename), ec);
    if (ec) {
        return;
    }

    fileWatcher->addWatch(fullpath.parent_path().u8string(), listener, false);
    callbacks[fullpath.u8string()].push_back(std::make_pair(filename, clb));
}

void watcher_check()
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
