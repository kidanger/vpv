#pragma once

#include <cstdint>

bool isKeyDown(const char* key);
bool isKeyPressed(const char* key, bool repeat=true);

void stopTime(uint64_t ms);
double /* in milliseconds */ letTimeFlow(uint64_t* t);

