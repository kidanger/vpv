#include <string>

#include "imgui.h"

#ifdef SDL
#include <SDL2/SDL.h>
#else
#include <SFML/System.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#endif

#include "events.hpp"

int getCode(const char* name) {
#ifdef SDL
#define specials(n, sdl, sfml) \
    if (std::string(name) == #n) { \
        return SDL_GetScancodeFromKey(SDLK_##sdl); \
    }
#else
#define specials(n, sdl, sfml) \
    if (std::string(name) == #n) { \
        return sf::Keyboard::sfml; \
    }
#endif

    specials(shift, LSHIFT, LShift);
    specials(alt, LALT, LAlt);
    specials(left, LEFT, Left);
    specials(right, RIGHT, Right);
    specials(up, UP, Up);
    specials(down, DOWN, Down);
    specials(control, LCTRL, LControl);
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
#ifndef SDL
        case 'a': return sf::Keyboard::A;
        case 'b': return sf::Keyboard::B;
        case 'c': return sf::Keyboard::C;
        case 'd': return sf::Keyboard::D;
        case 'e': return sf::Keyboard::E;
        case 'f': return sf::Keyboard::F;
        case 'g': return sf::Keyboard::G;
        case 'h': return sf::Keyboard::H;
        case 'i': return sf::Keyboard::I;
        case 'j': return sf::Keyboard::J;
        case 'k': return sf::Keyboard::K;
        case 'l': return sf::Keyboard::L;
        case 'm': return sf::Keyboard::M;
        case 'n': return sf::Keyboard::N;
        case 'o': return sf::Keyboard::O;
        case 'p': return sf::Keyboard::P;
        case 'q': return sf::Keyboard::Q;
        case 'r': return sf::Keyboard::R;
        case 's': return sf::Keyboard::S;
        case 't': return sf::Keyboard::T;
        case 'u': return sf::Keyboard::U;
        case 'v': return sf::Keyboard::V;
        case 'w': return sf::Keyboard::W;
        case 'x': return sf::Keyboard::X;
        case 'y': return sf::Keyboard::Y;
        case 'z': return sf::Keyboard::Z;
        case ' ': return sf::Keyboard::Space;
        case '\t': return sf::Keyboard::Tab;
        case '\b': return sf::Keyboard::BackSpace;
        case ',': return sf::Keyboard::Comma;
        case '1': return sf::Keyboard::Num1;
        case '2': return sf::Keyboard::Num2;
        case '3': return sf::Keyboard::Num3;
        case '4': return sf::Keyboard::Num4;
        case '5': return sf::Keyboard::Num5;
        case '6': return sf::Keyboard::Num6;
        case '7': return sf::Keyboard::Num7;
        case '8': return sf::Keyboard::Num8;
        case '9': return sf::Keyboard::Num9;
        case '0': return sf::Keyboard::Num0;
#endif
#ifdef SDL
        default:
            if (!name[1])
                return SDL_GetScancodeFromKey(*name);
#endif
    }
    printf("unknown key '%s'\n", name);
    exit(0);
    return 0;
}

bool isKeyDown(const char* key)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return false;
    int code = getCode(key);
    return ImGui::IsKeyDown(code);
}

bool isKeyPressed(const char* key, bool repeat)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return false;
    int code = getCode(key);
    return ImGui::IsKeyPressed(code, repeat);
}

void stopTime(uint64_t ms)
{
#ifndef SDL
    sf::sleep(sf::milliseconds(ms));
#else
    SDL_Delay(ms);
#endif
}

double letTimeFlow(uint64_t* t)
{
#ifndef SDL
    static sf::Clock clock;
    int64_t current = clock.getElapsedTime().asMicroseconds();
    if (*t == 0)
        *t = current;
    double dt = (current - *t) / 1000.;
    *t = current;
#else
    uint64_t current = SDL_GetPerformanceCounter();
    if (*t == 0)
        *t = current;
    double dt = (current - *t) * 1000 / (double) SDL_GetPerformanceFrequency();
    *t = current;
#endif
    return dt;
}

