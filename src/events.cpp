#include <string>

#include <imgui.h>

#include <SDL.h>

#include "events.hpp"

static int getCode(const char* name) {
#define specials(n, sdl, sfml) \
    if (std::string(name) == #n) { \
        return SDL_SCANCODE_##sdl; \
    }

    specials(left, LEFT, Left);
    specials(right, RIGHT, Right);
    specials(up, UP, Up);
    specials(down, DOWN, Down);
    specials(F1, F1, F1);
    specials(F2, F2, F2);
    specials(F3, F3, F3);
    specials(F4, F4, F4);
    specials(F5, F5, F5);
    specials(F6, F6, F6);
    specials(F7, F7, F7);
    specials(F8, F8, F8);
    specials(F9, F9, F9);
    specials(F10, F10, F10);
    specials(F11, F11, F11);
    specials(F12, F12, F12);

#undef specials

    switch (*name) {
        default:
            {
                SDL_Keycode key = SDL_GetKeyFromName(name);
                if (key != SDLK_UNKNOWN)
                    return SDL_GetScancodeFromKey(key);
            }
    }
    printf("unknown key '%s'\n", name);
    exit(0);
    return 0;
}

bool isKeyDown(const char* key)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return false;
    if (std::string(key) == "control") {
        return ImGui::GetIO().KeyCtrl;
    }
    if (std::string(key) == "shift") {
        return ImGui::GetIO().KeyShift;
    }
    if (std::string(key) == "alt") {
        return ImGui::GetIO().KeyAlt;
    }
    if (std::string(key) == "super") {
        return ImGui::GetIO().KeySuper;
    }
    int code = getCode(key);
    return ImGui::IsKeyDown(code);
}

bool isKeyPressed(const char* key, bool repeat)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return false;
    if (std::string(key) == "control" || std::string(key) == "shift"
        || std::string(key) == "alt" || std::string(key) == "super") {
        fprintf(stderr, "isKeyPressed: '%s' can only be used with isKeyDown \n", key);
    }
    int code = getCode(key);
    return ImGui::IsKeyPressed(code, repeat);
}

bool isKeyReleased(const char* key)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return false;
    if (std::string(key) == "control" || std::string(key) == "shift"
        || std::string(key) == "alt" || std::string(key) == "super") {
        fprintf(stderr, "isKeyReleased: '%s' can only be used with isKeyDown \n", key);
    }
    int code = getCode(key);
    return ImGui::IsKeyReleased(code);
}

void stopTime(uint64_t ms)
{
    SDL_Delay(ms);
}

double letTimeFlow(uint64_t* t)
{
    uint64_t current = SDL_GetPerformanceCounter();
    if (*t == 0)
        *t = current;
    double dt = (current - *t) * 1000 / (double) SDL_GetPerformanceFrequency();
    *t = current;
    return dt;
}

