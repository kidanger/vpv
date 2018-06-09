#pragma once

#include <vector>
#include <array>

struct Sequence;
struct View;
struct Player;
struct Window;
struct Colormap;
struct Shader;

extern std::vector<Sequence*> gSequences;
extern std::vector<View*> gViews;
extern std::vector<Player*> gPlayers;
extern std::vector<Window*> gWindows;
extern std::vector<Colormap*> gColormaps;
extern std::vector<Shader*> gShaders;

extern bool gUseCache;
extern bool gAsync;

#include "imgui.h"
extern bool gSelecting;
extern ImVec2 gSelectionFrom;
extern ImVec2 gSelectionTo;
extern bool gSelectionShown;

extern ImVec2 gHoveredPixel;

extern bool gShowHud;
extern std::array<bool, 9> gShowSVGs;
extern bool gShowMenu;
extern bool gShowImage;

extern ImVec2 gDefaultSvgOffset;
extern float gDefaultFramerate;
extern int gDownsamplingQuality;
extern int gCacheLimitMB;
extern bool gPreload;

extern int gActive;

