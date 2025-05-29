#include <string>

#include <imgui.h>

#include <SDL3/SDL.h>

#include "events.hpp"

int getCode(const char* name)
{
#define specials(n, sdl)                         \
    if (std::string(name) == #n) {               \
        return SDLK_##sdl & ~SDLK_SCANCODE_MASK; \
    }

    specials(left, LEFT);
    specials(right, RIGHT);
    specials(up, UP);
    specials(down, DOWN);
    specials(F1, F1);
    specials(F2, F2);
    specials(F3, F3);
    specials(F4, F4);
    specials(F5, F5);
    specials(F6, F6);
    specials(F7, F7);
    specials(F8, F8);
    specials(F9, F9);
    specials(F10, F10);
    specials(F11, F11);
    specials(F12, F12);
    specials(1, 1);
    specials(2, 2);
    specials(3, 3);
    specials(4, 4);
    specials(5, 5);
    specials(6, 6);
    specials(7, 7);
    specials(8, 8);
    specials(9, 9);

#undef specials

    switch (*name) {
    default: {
        SDL_Keycode key = SDL_GetKeyFromName(name);
        if (key != SDLK_UNKNOWN) {
            key &= ~SDLK_SCANCODE_MASK;
            if (key < 512)
                return key;
        }
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
    double dt = (current - *t) * 1000 / (double)SDL_GetPerformanceFrequency();
    *t = current;
    return dt;
}
