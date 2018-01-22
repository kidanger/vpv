#pragma once

#include <string>

void watcher_initialize(void);

void watcher_add_file(const std::string& filename, void(*clb)(const std::string& filename));

void watcher_check(void);

