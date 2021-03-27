#pragma once

#include <vector>
#include <array>

#define SEQUENCE_SEPARATOR ("::")

struct Sequence;
struct View;
struct Player;
struct Window;
struct Colormap;
struct Shader;
class Terminal;

extern std::vector<Sequence*> gSequences;
extern std::vector<View*> gViews;
extern std::vector<Player*> gPlayers;
extern std::vector<Window*> gWindows;
extern std::vector<Colormap*> gColormaps;
extern std::vector<Shader*> gShaders;
extern Terminal& gTerminal;

extern bool gUseCache;

#include "imgui.h"
extern bool gSelecting;
extern ImVec2 gSelectionFrom;
extern ImVec2 gSelectionTo;
extern bool gSelectionShown;

extern ImVec2 gHoveredPixel;

extern bool gShowHud;
extern std::array<bool, 9> gShowSVGs;
extern bool gShowHistogram;
extern bool gShowMenuBar;
extern bool gShowImage;
extern int gShowWindowBar;
extern int gWindowBorder;
extern bool gShowMiniview;

extern ImVec2 gDefaultSvgOffset;
extern float gDefaultFramerate;
extern int gDownsamplingQuality;
extern size_t gCacheLimitMB;
extern bool gPreload;
extern bool gSmoothHistogram;
extern bool gForceIioOpen;

extern int gActive;
extern int gShowView;
#define MAX_SHOWVIEW 70
extern bool gReloadImages;

