#pragma once

#include <cstdint>

bool isKeyDown(const char* key);
bool isKeyPressed(const char* key, bool repeat=true);
bool isKeyReleased(const char* key);

void stopTime(uint64_t ms);
double /* in milliseconds */ letTimeFlow(uint64_t* t);

