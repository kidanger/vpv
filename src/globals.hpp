#pragma once

#include <array>
#include <memory>
#include <vector>

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
// Above this level-0 footprint, a GDAL image is read chunk by chunk instead of
// being loaded whole. gForceBigMode does it whatever the size (for testing).
// See bigimages.md.
extern size_t gBigImageThresholdMB;
extern bool gForceBigMode;
// Largest view we are willing to load, as the side of a square, in pixels of
// the pyramid level being displayed. Past that we draw nothing rather than
// faulting in an unbounded number of chunks. See bigimages.md.
extern size_t gMaxViewportSize;
// How many pixels per band the range/quantile estimation of a big image may
// read out of the coarsest pyramid level. See bigimages.md.
extern size_t gStatsMaxPixels;
// Saturation cuts offered by alt+a, from SATURATIONS. They are precomputed by
// the statistics pass of a lazy image, so this has to be known in C++.
extern std::vector<float> gSaturations;
extern bool gSmoothHistogram;
extern bool gForceIioOpen;

extern int gActive;
extern int gShowView;
#define MAX_SHOWVIEW 70
extern bool gReloadImages;
extern bool gShowHelp;

extern std::string gPythonExe;
extern int gPythonTimeout;
extern std::string gPythonPreamble;
