#pragma once

#include <vector>
#include <array>
#include <memory>

#define SEQUENCE_SEPARATOR ("::")

struct Sequence;
struct View;
struct Player;
struct Window;
struct Colormap;
class Terminal;

extern std::vector<std::shared_ptr<Sequence>> gSequences;
extern std::vector<std::shared_ptr<View>> gViews;
extern std::vector<std::shared_ptr<Player>> gPlayers;
extern std::vector<std::shared_ptr<Window>> gWindows;
extern std::vector<std::shared_ptr<Colormap>> gColormaps;
extern Terminal& gTerminal;

#include <imgui.h>
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

extern float gDefaultFramerate;
extern int gDownsamplingQuality;
extern size_t gCacheLimitMB;
extern bool gSmoothHistogram;
extern bool gForceIioOpen;

extern int gActive;
extern int gShowView;
#define MAX_SHOWVIEW 70
extern bool gReloadImages;
extern bool gShowHelp;

