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

#include "Sequence.hpp"
#include "Player.hpp"
#include "View.hpp"
#include "Colormap.hpp"
#include "Image.hpp"
#include "ImageProvider.hpp"
#include "ImageCollection.hpp"
#include "alphanum.hpp"
#include "globals.hpp"
#include "SVG.hpp"
#include "editors.hpp"
#include "shaders.hpp"

Sequence::Sequence()
{
    static int id = 0;
    id++;
    ID = "Sequence " + std::to_string(id);

    view = nullptr;
    player = nullptr;
    colormap = nullptr;
    image = nullptr;
    imageprovider = nullptr;
    collection = nullptr;
    uneditedCollection= nullptr;

    valid = false;

    loadedFrame = -1;

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

static bool is_file(const std::string& filename)
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
    std::vector<std::string> filenames;
    recursive_collect(filenames, std::string(glob.c_str()));

    if (filenames.empty() && !strcmp(glob.c_str(), "-")) {
        filenames.push_back("-");
    }

    MultipleImageCollection* collection = new MultipleImageCollection();
    for (auto& f : filenames) {
        collection->append(new SingleImageImageCollection(f));
    }
    this->collection = collection;
    this->uneditedCollection = collection;

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

void Sequence::tick()
{
    if (valid && player && loadedFrame != player->frame && image) {
        forgetImage();
    }

    if (!imageprovider) {
        forgetImage();
    }

    if (imageprovider && imageprovider->isLoaded()) {
        ImageProvider::Result result = imageprovider->getResult();
        if (result.isOK) {
            image = result.value;
        } else {
            printf("%s\n", result.error->what());
            forgetImage();
        }
    }

    if (image && colormap && !colormap->initialized) {
        colormap->autoCenterAndRadius(image->min, image->max);

        if (!colormap->shader) {
            switch (image->format) {
                case 1:
                    colormap->shader = getShader("gray");
                    break;
                case 2:
                    colormap->shader = getShader("opticalFlow");
                    break;
                default:
                case 4:
                case 3:
                    colormap->shader = getShader("default");
                    break;
            }
        }
        colormap->initialized = true;
    }
}

void Sequence::forgetImage()
{
    if (image && !image->is_cached) {
        delete image;
    }
    image = nullptr;
    imageprovider = collection->getImageProvider(player->frame - 1);
    loadedFrame = player->frame;
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

    colormap->autoCenterAndRadius(0., dynamics[best]-1);
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

const Image* Sequence::getCurrentImage() {
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
    title += "[" + std::to_string(player->frame) + '/' + std::to_string(collection->getLength()) + "]";
    title += " " + collection->getFilename(player->frame - 1);
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
        ImGui::Text("Size: %lux%lux%lu", image->w, image->h, image->format);
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

void Sequence::setEdit(const std::string& edit, EditType edittype)
{
    this->edittype = edittype;
    strcpy(this->editprog, edit.c_str());
    if (edit.empty()) {
        collection = uneditedCollection;
    } else {
        collection = create_edited_collection(edittype, std::string(editprog));
    }
    forgetImage();
}

std::string Sequence::getEdit()
{
    return std::string(this->editprog);
}

int Sequence::getId()
{
    int id = 0;
    while (gSequences[id] != this && id < gSequences.size())
        id++;
    id++;
    return id;
}

