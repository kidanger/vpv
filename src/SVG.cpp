#include <string>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mutex>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION	// Expands implementation
#include "SVG.hpp"
#include "watcher.hpp"
#include "globals.hpp"

std::unordered_map<std::string, SVG*> SVG::cache;
static std::mutex lock;

SVG::SVG()
    : nsvg(nullptr), filename(""), valid(false)
{
}

void SVG::loadFromFile(const std::string& filename)
{
    this->filename = filename;
    valid = (nsvg = nsvgParseFromFile(filename.c_str(), "px", 96));
}

void SVG::loadFromString(const std::string& str)
{
    filename = "buffer";
    std::string copy(str);
    valid = (nsvg = nsvgParse(&copy[0], "px", 96));
}

SVG::~SVG()
{
    if (nsvg)
        nsvgDelete(nsvg);
}

void SVG::draw(ImVec2 basepos, ImVec2 pos, float zoom) const
{
    if (!nsvg || !valid)
        return;

    const auto adjust = [basepos,pos,zoom](float x, float y, bool relative) {
        if (relative)
            return ImVec2(x,y) * zoom + pos + basepos;
        return ImVec2(x,y) + basepos;
    };
    const auto curveisflat = [](float* p1_, float* p2_, float* p3_) {
        auto p1 = ImVec2(p1_[0], p1_[1]);
        auto p2 = ImVec2(p2_[0], p2_[1]);
        auto p3 = ImVec2(p3_[0], p3_[1]);
        return fabs((p1.y - p2.y) * (p1.x - p3.x) - (p1.y - p3.y) * (p1.x - p2.x)) <= 1e-1;
    };

    auto dl = ImGui::GetWindowDrawList();
    for (auto shape = nsvg->shapes; shape != NULL; shape = shape->next) {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE))
            continue;

        bool rel = shape->flags & NSVG_FLAGS_RELATIVE;
        ImU32 fillColor = shape->fill.color;
        ImU32 strokeColor = shape->stroke.color;
        float strokeWidth = shape->strokeWidth * (rel?zoom:1.f);
        if (shape->strokeWidth < 0) {
            strokeWidth = -shape->strokeWidth;
        }

        if (shape->isText) {
            dl->AddText(nullptr, shape->fontSize*(rel?zoom:1), adjust(shape->paths->pts[0], shape->paths->pts[1], rel),
                        fillColor, shape->textData);
            continue;
        }

        for (auto path = shape->paths; path != NULL; path = path->next) {
            dl->PathClear();
            dl->PathLineTo(adjust(path->pts[0], path->pts[1], rel));

            bool closed = path->closed;
            int npts = path->npts;

            // trim the path if it is looping back (polygon), so that we can use the closing of imgui
            // however, if the path is not flat at the end (circle), then don't trim
#define DIST(i, j) (hypot(path->pts[(i)*2] - path->pts[(j)*2], path->pts[(i)*2+1] - path->pts[(j)*2+1]))
            float dist = DIST(0, npts-1);
            while (dist < 1e-5f && curveisflat(&path->pts[(npts-3)*2], &path->pts[(npts-2)*2], &path->pts[(npts-1)*2])) {
                closed = true;
                npts -= 3;  // remove the last bezier curve
                dist = DIST(0, npts-1);
            }
#undef DIST
            if (dist < 1e-5f) {
                closed = false;
            }

            for (int i = 0; i < npts-1; i += 3) {
                float* p = &path->pts[i*2];
                dl->PathBezierCurveTo(adjust(p[2], p[3], rel), adjust(p[4], p[5], rel), adjust(p[6], p[7], rel));
            }

            if (shape->stroke.type)
                dl->PathStroke(strokeColor, closed, strokeWidth);
            if (shape->fill.type && dl->_Path.Size)
                dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, fillColor);
        }
    }
    dl->PathClear();
}

SVG* SVG::get(const std::string& filename)
{
    lock.lock();
    auto i = cache.find(filename);
    if (i != cache.end()) {
        SVG* svg = i->second;
        lock.unlock();
        return svg;
    }
    lock.unlock();

    SVG* svg = new SVG;
    svg->loadFromFile(filename);
    lock.lock();
    cache[filename] = svg;
    lock.unlock();
    if (svg->valid) {
        //printf("'%s' loaded\n", filename.c_str());
    } else {
        printf("'%s' invalid\n", filename.c_str());
    }

    watcher_add_file(filename, [&](const std::string& f) {
        lock.lock();
        auto entry = cache.find(filename);
        if (entry != cache.end()) {
            SVG* svg = entry->second;
            delete svg;
            cache.erase(entry);
        }
        lock.unlock();
        //printf("'%s' modified on disk, cache invalidated\n", filename.c_str());
        gActive = std::max(gActive, 2);
    });

    return svg;
}

std::shared_ptr<SVG> SVG::createFromString(const std::string& str)
{
    struct sharablesvg : public SVG {};
    std::shared_ptr<SVG> svg = std::make_shared<sharablesvg>();
    svg->loadFromString(str);
    return svg;
}

void SVG::flushCache()
{
    for (const auto& v : cache) {
        delete v.second;
    }
    cache.clear();
}

