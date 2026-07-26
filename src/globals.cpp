#include <array>
#include <memory>
#include <vector>

#include "Terminal.hpp"
#include "globals.hpp"

std::vector<std::shared_ptr<Sequence>> gSequences;
std::vector<std::shared_ptr<View>> gViews;
std::vector<std::shared_ptr<Player>> gPlayers;
std::vector<std::shared_ptr<Window>> gWindows;
std::vector<std::shared_ptr<Colormap>> gColormaps;
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
size_t gGpuCacheLimitMB;
size_t gGdalCacheLimitMB;
size_t gBigImageThresholdMB;
bool gForceBigMode;
size_t gChunkLoaderThreads;
size_t gMaxViewportSize;
size_t gStatsMaxPixels;
std::vector<float> gSaturations;
bool gSmoothHistogram;
bool gForceIioOpen;
int gActive;
int gShowView;
bool gReloadImages;
bool gShowHelp = false;

Terminal term;
Terminal& gTerminal = term;

std::string gPythonExe;
int gPythonTimeout;
std::string gPythonPreamble;
