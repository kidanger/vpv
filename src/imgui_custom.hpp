#pragma once

#include <array>
#include <memory>

#include <imgui.h>

#include "Shader.hpp"

namespace ImGui {

struct ShaderUserData {
    std::shared_ptr<Shader::Program> shader;
    std::array<float, 3> scale;
    std::array<float, 3> bias;
};

void SetShaderCallback(const ImDrawList* parent_list, const ImDrawCmd* pcmd);

void PlotMultiLines(const char* label,
    int num_datas,
    const char** names,
    const ImColor* colors,
    float (*getter)(const void* data, int idx),
    const void* const* datas,
    int values_count,
    float scale_min,
    float scale_max,
    ImVec2 graph_size,
    const int* boundsmin, const int* boundsmax, const int* highlights);

void PlotMultiHistograms(const char* label,
    int num_hists,
    const char** names,
    const ImColor* colors,
    float (*getter)(const void* data, int idx),
    const void* const* datas,
    int values_count,
    float scale_min,
    float scale_max,
    ImVec2 graph_size,
    const int* boundsmin, const int* boundsmax, const int* highlights);

bool BufferingBar(const char* label, float value,
    const ImVec2& size_arg, const ImU32& bg_col,
    const ImU32& fg_col);
}
