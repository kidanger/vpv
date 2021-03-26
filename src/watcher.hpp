#pragma once

#include <string>
#include <functional>

void watcher_initialize();

void watcher_add_file(const std::string& filename, const std::function<void(const std::string&)>& clb);

void watcher_check();

