#include <cstdlib>
#include <cstring>
#include <glob.h>
#include <algorithm>
#include <limits>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <sys/types.h> // stat
#include <sys/stat.h> // stat

#ifndef SDL
#include <SFML/OpenGL.hpp>
#include <GL/glew.h>
#else
#include <GL/gl3w.h>
#endif

#include "Sequence.hpp"
#include "Player.hpp"
#include "View.hpp"
#include "Colormap.hpp"
#include "Image.hpp"
#include "alphanum.hpp"
#include "globals.hpp"
#include "SVG.hpp"
#include "editors.hpp"

const char* getGLError(GLenum error)
{
#define casereturn(x) case x: return #x
    switch (error) {
        casereturn(GL_INVALID_ENUM);
        casereturn(GL_INVALID_VALUE);
        casereturn(GL_INVALID_OPERATION);
        casereturn(GL_INVALID_FRAMEBUFFER_OPERATION);
        casereturn(GL_OUT_OF_MEMORY);
        default:
        case GL_NO_ERROR:
        return "";
    }
#undef casereturn
}

#define GLDEBUG(x) \
    x; \
{ \
    GLenum e; \
    while((e = glGetError()) != GL_NO_ERROR) \
    { \
        printf("%s:%s:%d for call %s", getGLError(e), __FILE__, __LINE__, #x); \
    } \
}

Sequence::Sequence()
{
    static int id = 0;
    id++;
    ID = "Sequence " + std::to_string(id);

    view = nullptr;
    player = nullptr;
    colormap = nullptr;
    image = nullptr;

    valid = false;
    force_reupload = false;

    loadedFrame = -1;
    loadedRect = ImRect();

    glob.reserve(2<<18);
    glob_.reserve(2<<18);
    glob = "";
    glob_ = "";

    editprog[0] = 0;
}

Sequence::~Sequence()
{
    forgetImage();
}

// from https://stackoverflow.com/a/236803
template<typename Out>
static void split(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

bool is_file(const std::string& filename)
{
    struct stat info;
    return !stat(filename.c_str(), &info) && !(info.st_mode & S_IFDIR);  // let's assume any non-dir is a file
}

static void recursive_collect(std::vector<std::string>& filenames, std::string glob)
{
    std::vector<std::string> collected;

    glob_t res;
    ::glob(glob.c_str(), GLOB_TILDE | GLOB_NOSORT | GLOB_BRACE, NULL, &res);
    bool found = false;
    for(unsigned int j = 0; j < res.gl_pathc; j++) {
        std::string file(res.gl_pathv[j]);
        collected.push_back(file);
        found = found || is_file(file);
    }
    globfree(&res);

    if (collected.size() == 1 && !found /* it's a directory */) {
        std::string dirglob = collected[0] + (collected[0][collected[0].length()-1] != '/' ? "/*" : "*");
        recursive_collect(filenames, dirglob);
        found = true;
    } else {
        std::sort(collected.begin(), collected.end(), doj::alphanum_less<std::string>());
        for (auto str : collected)
            filenames.push_back(str);
    }

    if (!found) {
        std::vector<std::string> substr;
        split(glob, ':', std::back_inserter(substr));
        if (substr.size() >= 2) {
            for (const std::string& s : substr) {
                recursive_collect(filenames, s);
            }
        }
    }
}

void Sequence::loadFilenames() {
    filenames.resize(0);
    recursive_collect(filenames, std::string(glob.c_str()));

    if (filenames.empty() && !strcmp(glob.c_str(), "-")) {
        filenames.push_back("-");
    }

    valid = filenames.size() > 0;
    strcpy(&glob_[0], &glob[0]);

    loadedFrame = -1;
    if (player)
        player->reconfigureBounds();

    svgcollection.resize(svgglobs.size());
    for (int j = 0; j < svgglobs.size(); j++) {
        if (!strcmp(svgglobs[j].c_str(), "auto")) {
            svgcollection[j] = filenames;
            for (int i = 0; i < svgcollection[j].size(); i++) {
                std::string filename = svgcollection[j][i];
                int h;
                for (h = filename.size()-1; h > 0 && filename[h] != '.'; h--)
                    ;
                filename.resize(h);
                filename = filename + ".svg";
                svgcollection[j][i] = filename;
            }
        } else {
            svgcollection[j].resize(0);
            recursive_collect(svgcollection[j], std::string(svgglobs[j].c_str()));
        }
    }
}

void Sequence::loadTextureIfNeeded()
{
    if (valid && player) {
        if (loadedFrame != player->frame || force_reupload) {
            forgetImage();
        }
    }
}

void Sequence::forgetImage()
{
    loadedRect = ImRect();
    if (image && !image->is_cached) {
        delete image;
    }
    image = nullptr;
    force_reupload = true;
}

void Sequence::requestTextureArea(ImRect rect)
{
    int frame = player->frame;
    assert(frame > 0);

    if (frame != loadedFrame) {
        forgetImage();
    }

    const Image* img = getCurrentImage();
    if (!img)
        return;

    int w = img->w;
    int h = img->h;

    rect.ClipWithFull(ImRect(0, 0, w, h));

    bool reupload = force_reupload;
    if (!loadedRect.Contains(rect)) {
        loadedRect.Add(rect);
        reupload = true;

        loadedRect.Expand(128);  // to avoid multiple uploads during zoom-out
        loadedRect.ClipWithFull(ImRect(0, 0, w, h));
    }

    if (reupload) {
        unsigned int gltype = GL_FLOAT;
        size_t elsize = sizeof(float);

        unsigned int glformat;
        if (img->format == Image::R)
            glformat = GL_RED;
        else if (img->format == Image::RG)
            glformat = GL_RG;
        else if (img->format == Image::RGB)
            glformat = GL_RGB;
        else if (img->format == Image::RGBA)
            glformat = GL_RGBA;
        else
            assert(0);

        if (texture.id == -1 || texture.size.x != w || texture.size.y != h
            || texture.type != gltype || texture.format != glformat) {
            texture.create(w, h, gltype, glformat);
        }

        auto area = loadedRect;
        const uint8_t* data = (uint8_t*) img->pixels + elsize*(w * (int)area.Min.y + (int)area.Min.x)*img->format;

        if (area.GetWidth() > 0 && area.GetHeight() > 0) {
            glBindTexture(GL_TEXTURE_2D, texture.id);
            GLDEBUG();

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            GLDEBUG();
            glTexSubImage2D(GL_TEXTURE_2D, 0, area.Min.x, area.Min.y, area.GetWidth(), area.GetHeight(),
                            glformat, gltype, data);
            GLDEBUG();
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            GLDEBUG();

            if (gDownsamplingQuality >= 2) {
                glGenerateMipmap(GL_TEXTURE_2D);
                GLDEBUG();
            }
        }

        loadedFrame = frame;
        force_reupload = false;
    }
}

void Sequence::autoScaleAndBias()
{
    for (int i = 0; i < 3; i++)
        colormap->center[i] = .5f;
    colormap->radius = .5f;

    const Image* img = getCurrentImage();
    if (!img)
        return;

    colormap->autoCenterAndRadius(img->min, img->max);
}

void Sequence::snapScaleAndBias()
{
    const Image* img = getCurrentImage();
    if (!img)
        return;

    double min = img->min;
    double max = img->max;

    double dynamics[] = {1., std::pow(2, 8), std::pow(2, 16), std::pow(2, 32)};
    int best = 0;

    for (int d = sizeof(dynamics)/sizeof(double) - 1; d >= 0; d--) {
        if (min > -dynamics[d]/1.5 && max - min < dynamics[d]*2.)
            best = d;
    }

    colormap->autoCenterAndRadius(0., dynamics[best]);
}

void Sequence::localAutoScaleAndBias(ImVec2 p1, ImVec2 p2)
{
    for (int i = 0; i < 3; i++)
        colormap->center[i] = .5f;
    colormap->radius = .5f;

    const Image* img = getCurrentImage();
    if (!img)
        return;

    if (p1.x < 0) p1.x = 0;
    if (p1.y < 0) p1.y = 0;
    if (p2.x >= img->w-1) p2.x = img->w - 1;
    if (p2.y >= img->h-1) p2.y = img->h - 1;

    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    const float* data = (const float*) img->pixels;
    for (int y = p1.y; y < p2.y; y++) {
        for (int x = p1.x; x < p2.x; x++) {
            for (int d = 0; d < img->format; d++) {
                float v = data[d + img->format*(x+y*img->w)];
                if (std::isfinite(v)) {
                    min = std::min(min, v);
                    max = std::max(max, v);
                }
            }
        }
    }

    colormap->autoCenterAndRadius(min, max);
}

void Sequence::cutScaleAndBias(float percentile)
{
    for (int i = 0; i < 3; i++)
        colormap->center[i] = .5f;
    colormap->radius = .5f;

    const Image* img = getCurrentImage();
    if (!img)
        return;

    const float* data = (const float*) img->pixels;
    std::vector<float> sorted(data, data+img->w*img->h*img->format);
    std::remove_if(sorted.begin(), sorted.end(), [](float x){return std::isfinite(x);});
    std::sort(sorted.begin(), sorted.end());

    float min = sorted[percentile*sorted.size()];
    float max = sorted[(1-percentile)*sorted.size()];
    colormap->autoCenterAndRadius(min, max);
}

Image* run_edit_program(char* prog, EditType edittype)
{
    std::vector<Sequence*> sequences;
    while (*prog && *prog != ' ') {
        char* old = prog;
        int a = strtol(prog, &prog, 10) - 1;
        if (prog == old) break;
        if (a < 0 || a >= gSequences.size()) return 0;
        sequences.push_back(gSequences[a]);
        if (*prog == ' ') break;
        if (*prog) prog++;
    }
    while (*prog == ' ') prog++;

    std::vector<const Image*> images;
    for (auto seq : sequences) {
        const Image* img = seq->getCurrentImage(true, true);
        if (!img) {
            return 0;
        }
        images.push_back(img);
    }

    return edit_images(edittype, prog, images);
}

const Image* Sequence::getCurrentImage(bool noedit, bool force) {
    if (!valid || !player) {
        return 0;
    }

    if (!image || noedit) {
        int frame = player->frame - 1;
        if (frame < 0 || frame >= filenames.size())
            return 0;

        const Image* img = Image::load(filenames[frame], force || !gAsync);
        if (!img && gAsync) {
            future = std::async([&](std::string filename) {
                Image::load(filename, true);
                gActive = std::max(gActive, 2);
                force_reupload = true;
            }, filenames[frame]);
            return 0;
        }
        image = img;

        if (!noedit && editprog[0]) {
            image = run_edit_program(editprog, edittype);
            if (!image)
                image = img;
        }
    }

    return image;
}

float Sequence::getViewRescaleFactor() const
{
    if (!this->view->shouldRescale) {
        return 1.;
    }

    if (!this->view || !this->image) {
        return previousFactor;
    }

    int largestW = image->w;
    for (auto& seq : gSequences) {
        if (view == seq->view && seq->image && largestW < seq->image->w) {
            largestW = seq->image->w;
        }
    }
    previousFactor = (float) largestW / image->w;
    return previousFactor;
}

std::vector<const SVG*> Sequence::getCurrentSVGs() const
{
    std::vector<const SVG*> svgs;
    if (!player) goto end;
    for (auto& svgfilenames : svgcollection) {
        if (svgfilenames.empty()) {
            continue;
        }
        int frame = player->frame - 1;
        if (player->frame > svgfilenames.size()) {
            frame = 0;
        }
        svgs.push_back(SVG::get(svgfilenames[frame]));
    }
end:
    return svgs;
}

const std::string Sequence::getTitle() const
{
    std::string seqname = std::string(glob.c_str());
    if (!valid)
        return "(the sequence '" + seqname + "' contains no images)";
    if (!player)
        return "(no player associated with the sequence '" + seqname + "')";
    if (!colormap)
        return "(no colormap associated with the sequence '" + seqname + "')";

    std::string title;
    int id = 0;
    while (gSequences[id] != this && id < gSequences.size())
        id++;
    id++;
    title += "#" + std::to_string(id) + " ";
    title += "[" + std::to_string(player->frame) + '/' + std::to_string(filenames.size()) + "]";
    title += " " + filenames[player->frame - 1];
    if (!image) {
        title += " cannot be loaded";
    }
    return title;
}

void Sequence::showInfo() const
{
    if (!valid || !player || !colormap)
        return;

    std::string seqname = std::string(glob.c_str());

    if (image) {
        int i = 0;
        for (auto svg : getCurrentSVGs()) {
            ImGui::Text("SVG %d: %s%s", i+1, svg->filename.c_str(), (!svg->valid ? " invalid" : ""));
            i++;
        }
        ImGui::Text("Size: %dx%dx%d", image->w, image->h, image->format);
        ImGui::Text("Range: %g..%g", image->min, image->max);
        ImGui::Text("Zoom: %d%%", (int)(view->zoom*100));
        ImGui::Separator();

        float cmin, cmax;
        colormap->getRange(cmin, cmax, image->format);
        ImGui::Text("Displayed: %g..%g", cmin, cmax);
        ImGui::Text("Shader: %s", colormap->getShaderName().c_str());
        if (*editprog) {
            const char* name;
            switch (edittype) {
                case PLAMBDA: name = "plambda"; break;
                case GMIC: name = "gmic"; break;
                case OCTAVE: name = "octave"; break;
            }
            ImGui::Text("Edited with %s", name);
        }
    }
}

