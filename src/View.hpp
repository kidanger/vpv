#pragma once

#include <memory>
#include <string>

#include <imgui.h>

struct View {
    std::string ID;

    float zoom;
    ImVec2 center;
    bool shouldRescale;
    ImVec2 svgOffset;

    View();

    bool operator==(const View& other);

    void resetZoom();
    void changeZoom(float zoom);
    void setOptimalZoom(ImVec2 winSize, ImVec2 texSize, float zoomfactor);

    ImVec2 image2window(const ImVec2& im, const ImVec2& imSize, const ImVec2& winSize, float zoomfactor) const;
    ImVec2 window2image(const ImVec2& win, const ImVec2& imSize, const ImVec2& winSize, float zoomfactor) const;

    void displaySettings();

    void onSequenceAttach(std::weak_ptr<struct Sequence> s) { }
    void onSequenceDetach(std::weak_ptr<struct Sequence> s) { }

    bool parseArg(const std::string& arg);
};
