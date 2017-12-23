#include <string>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION	// Expands implementation
#include "SVG.hpp"

SVG::~SVG()
{
    if (nsvg)
        nsvgDelete(nsvg);
}

bool SVG::load(const std::string& filename)
{
    assert(!nsvg);

    this->filename = filename;
    valid = (nsvg = nsvgParseFromFile(filename.c_str(), "px", 96));
    return valid;
}

void SVG::draw(ImVec2 pos, float zoom) const
{
    assert(nsvg);
    assert(valid);

    const auto adjust = [pos,zoom](float x, float y) {
        return ImVec2(x,y) * zoom + pos;
    };

    auto dl = ImGui::GetWindowDrawList();
    for (auto shape = nsvg->shapes; shape != NULL; shape = shape->next) {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE))
            continue;

        ImU32 fillColor = shape->fill.color;
        ImU32 strokeColor = shape->stroke.color;
        float strokeWidth = shape->strokeWidth;

        if (shape->isText) {
            dl->AddText(nullptr, shape->fontSize*zoom, adjust(shape->paths->pts[0], shape->paths->pts[1]),
                        fillColor, shape->textData);
            continue;
        }

        for (auto path = shape->paths; path != NULL; path = path->next) {
            dl->PathClear();
            dl->PathLineTo(adjust(path->pts[0], path->pts[1]));

            for (int i = 0; i < path->npts-1; i += 3) {
                float* p = &path->pts[i*2];
                dl->PathBezierCurveTo(adjust(p[2], p[3]), adjust(p[4], p[5]), adjust(p[6], p[7]));
            }

            if (path->closed)
                dl->PathLineTo(adjust(path->pts[0], path->pts[1]));
            if (shape->fill.type)
                dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, fillColor, true);
            if (shape->stroke.type)
                dl->PathStroke(strokeColor, false, strokeWidth);
        }
    }
    dl->PathClear();
}

