#pragma once


#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"


namespace ImGui {

    static ImU32 InvertColorU32(ImU32 in);


    static void PlotMultiEx(
            ImGuiPlotType plot_type,
            const char* label,
            int num_datas,
            const char** names,
            const ImColor* colors,
            float(*getter)(const float* data, int idx, int idx2, int stride),
            float *datas,
            int values_count,
            float scale_min,
            float scale_max,
            ImVec2 graph_size, int stride, float max = -1);

    void PlotMultiLines(
            const char* label,
            int num_datas,
            const char** names,
            const ImColor* colors,
            float(*getter)(const float* data, int idx, int idx2, int stride),
            float *datas,
            int values_count,
            float scale_min,
            float scale_max,
            ImVec2 graph_size, int stride, float max = -1);

    void PlotMultiHistograms(
            const char* label,
            int num_hists,
            const char** names,
            const ImColor* colors,
            float(*getter)(const float* data, int idx, int idx2, int stride),
            float *datas,
            int values_count,
            float scale_min,
            float scale_max,
            ImVec2 graph_size, int stride, float max = -1);
}
