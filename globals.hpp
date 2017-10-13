#pragma once

#include <vector>

namespace sf {
    class RenderWindow;
}

struct Sequence;
struct View;
struct Player;
struct Window;
struct Colormap;
struct Shader;

extern sf::RenderWindow* SFMLWindow;
extern std::vector<Sequence*> gSequences;
extern std::vector<View*> gViews;
extern std::vector<Player*> gPlayers;
extern std::vector<Window*> gWindows;
extern std::vector<Colormap*> gColormaps;
extern std::vector<Shader*> gShaders;

extern bool useCache;

#include "imgui.h"
extern bool gSelecting;
extern ImVec2 gSelectionFrom;
extern ImVec2 gSelectionTo;
extern bool gSelectionShown;

extern ImVec2 gHoveredPixel;

extern bool gShowHud;

