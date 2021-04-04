#include <vector>
#include <array>

#include "Terminal.hpp"
#include "globals.hpp"

std::vector<Sequence*> gSequences;
std::vector<View*> gViews;
std::vector<Player*> gPlayers;
std::vector<Window*> gWindows;
std::vector<Colormap*> gColormaps;
bool gSelecting;
ImVec2 gSelectionFrom;
ImVec2 gSelectionTo;
bool gSelectionShown;
ImVec2 gHoveredPixel;
bool gShowHud;
std::array<bool, 9> gShowSVGs;
bool gShowMenuBar;
bool gShowHistogram;
bool gShowMiniview;
int gShowWindowBar;
int gWindowBorder;
bool gShowImage;
float gDefaultFramerate;
int gDownsamplingQuality;
size_t gCacheLimitMB;
bool gSmoothHistogram;
bool gForceIioOpen;
int gActive;
int gShowView;
bool gReloadImages;
bool gShowHelp = false;

Terminal term;
Terminal& gTerminal = term;

