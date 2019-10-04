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
#include "Histogram.hpp"
#include "editors.hpp"
#include "shaders.hpp"
#include "EditGUI.hpp"

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
    editGUI = new EditGUI();

    valid = false;

    loadedFrame = -1;

    glob.reserve(2<<18);
    glob_.reserve(2<<18);
    glob = "";
    glob_ = "";
}

Sequence::~Sequence()
{
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

static bool is_directory(const std::string& filename)
{
    struct stat info;
    return !stat(filename.c_str(), &info) && (info.st_mode & S_IFDIR);
}


static void recursive_collect(std::vector<std::string>& filenames, std::string glob)
{
    // TODO: unit test all that
    std::vector<std::string> collected;

    glob_t res;
    ::glob(glob.c_str(), GLOB_TILDE | GLOB_NOSORT | GLOB_BRACE, NULL, &res);
    for(unsigned int j = 0; j < res.gl_pathc; j++) {
        std::string file(res.gl_pathv[j]);
        if (is_directory(file)) {
            std::string dirglob = file + (file[file.length()-1] != '/' ? "/*" : "*");
            recursive_collect(collected, dirglob);
        } else {
            collected.push_back(file);
        }
    }
    globfree(&res);

    if (!strncmp(glob.c_str(), "/vsi", 4)) {
        filenames.push_back(glob);
    } else if (collected.empty()) {
        std::vector<std::string> substr;
        split(glob, ':', std::back_inserter(substr));
        if (substr.size() >= 2) {
            for (const std::string& s : substr) {
                recursive_collect(filenames, s);
            }
        }
    } else {
        std::sort(collected.begin(), collected.end(), doj::alphanum_less<std::string>());
        for (auto str : collected) {
            filenames.push_back(str);
        }
    }
}

void Sequence::loadFilenames() {
    std::vector<std::string> filenames;
    recursive_collect(filenames, std::string(glob.c_str()));

    if (filenames.empty() && !strcmp(glob.c_str(), "-")) {
        filenames.push_back("-");
    }

    ImageCollection* col = buildImageCollectionFromFilenames(filenames);
    this->collection = col;
    this->uneditedCollection = col;

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

    forgetImage();
}

void Sequence::tick()
{
    if (valid && player && loadedFrame != player->frame && (image || !error.empty())) {
        forgetImage();
    }

    if (imageprovider && imageprovider->isLoaded()) {
        ImageProvider::Result result = imageprovider->getResult();
        if (result.has_value()) {
            image = result.value();
            error.clear();
            LOG("new image: " << image);
        } else {
            error = result.error();
            LOG("new error: " << error);
            forgetImage();
        }
        gActive = std::max(gActive, 2);
        imageprovider = nullptr;
        if (image) {
            image->histogram->request(image, image->min, image->max,
                                      gSmoothHistogram ? Histogram::SMOOTH : Histogram::EXACT);
        }
    }

    if (image && colormap && !colormap->initialized) {
        colormap->autoCenterAndRadius(image->min, image->max);

        if (!colormap->shader) {
            switch (image->c) {
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
    LOG("forget image, was=" << image << " provider=" << imageprovider);
    image = nullptr;
    if (player && collection && player->frame - 1 >= 0
        && player->frame - 1 < collection->getLength()) {
        imageprovider = collection->getImageProvider(player->frame - 1);
        loadedFrame = player->frame;
    }
    LOG("forget image, new provider=" << imageprovider);
}

void Sequence::autoScaleAndBias(ImVec2 p1, ImVec2 p2, float quantile)
{
    std::shared_ptr<Image> img = getCurrentImage();
    if (!img)
        return;

    float low = std::numeric_limits<float>::max();
    float high = std::numeric_limits<float>::lowest();
    bool norange = p1.x == p2.x && p1.y == p2.y && p1.x == 0 && p2.x == 0;
    if (p1.x < 0) p1.x = 0;
    if (p1.y < 0) p1.y = 0;
    if (p2.x >= img->w-1) p2.x = img->w - 1;
    if (p2.y >= img->h-1) p2.y = img->h - 1;

    if (quantile == 0) {
        if (norange) {
            low = img->min;
            high = img->max;
        } else {
            const float* data = (const float*) img->pixels;
            for (int y = p1.y; y < p2.y; y++) {
                for (int x = p1.x; x < p2.x; x++) {
                    for (int d = 0; d < img->c; d++) {
                        float v = data[d + img->c*(x+y*img->w)];
                        if (std::isfinite(v)) {
                            low = std::min(low, v);
                            high = std::max(high, v);
                        }
                    }
                }
            }
        }
    } else {
        std::vector<float> all;
        const float* data = (const float*) img->pixels;
        if (norange) {
            all = std::vector<float>(data, data+img->w*img->h*img->c);
        } else {
            for (int y = p1.y; y < p2.y; y++) {
                const float* start = &data[0 + img->c*((int)p1.x+y*img->w)];
                // XXX: might be off by one, who knows
                const float* end = &data[0 + img->c*((int)p2.x+y*img->w)];
                all.insert(all.end(), start, end);
            }
        }
        all.erase(std::remove_if(all.begin(), all.end(),
                                 [](float x){return !std::isfinite(x);}),
                  all.end());
        std::sort(all.begin(), all.end());
        low = all[quantile*all.size()];
        high = all[(1-quantile)*all.size()];
    }

    colormap->autoCenterAndRadius(low, high);
}

void Sequence::snapScaleAndBias()
{
    std::shared_ptr<Image> img = getCurrentImage();
    if (!img)
        return;

    double min = img->min;
    double max = img->max;

    double dynamics[] = {1., std::pow(2, 8)-1, std::pow(2, 16)-1, std::pow(2, 32)-1};
    int best = 0;

    for (int d = sizeof(dynamics)/sizeof(double) - 1; d >= 0; d--) {
        if (min > -dynamics[d]/1.5 && max - min < dynamics[d]*2.)
            best = d;
    }

    colormap->autoCenterAndRadius(0., dynamics[best]);
}

std::shared_ptr<Image> Sequence::getCurrentImage() {
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

    size_t largestW = image->w;
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

const std::string Sequence::getTitle(int ncharname) const
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
    std::string filename(collection->getFilename(player->frame - 1));
    int p = filename.size() - ncharname;
    if (p < 0 || ncharname == -1) p = 0;
    if (p < filename.size()) {
        title += " " + filename.substr(p);
    }
    if (!image) {
        if (imageprovider) {
            title += " is loading";
        } else {
            title += " cannot be loaded";
        }
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
        ImGui::Text("Size: %lux%lux%lu", image->w, image->h, image->c);
        ImGui::Text("Range: %g..%g", image->min, image->max);
        ImGui::Text("Zoom: %d%%", (int)(view->zoom*100));
        ImGui::Separator();

        if (colormap->initialized) {
            float cmin, cmax;
            colormap->getRange(cmin, cmax, image->c);
            ImGui::Text("Displayed: %g..%g", cmin, cmax);
            ImGui::Text("Shader: %s", colormap->getShaderName().c_str());
        } else {
            ImGui::Text("Colormap not initialized");
        }
        if (editGUI->isEditing()) {
            ImGui::Text("Edited with %s", editGUI->getEditorName().c_str());
        }
    }
}

void Sequence::setEdit(const std::string& edit, EditType edittype)
{
    editGUI->edittype = edittype;
    strncpy(editGUI->editprog, edit.c_str(), sizeof(editGUI->editprog));
    editGUI->validate(*this);
}

std::string Sequence::getEdit()
{
    return editGUI->editprog;
}

int Sequence::getId()
{
    int id = 0;
    while (gSequences[id] != this && id < gSequences.size())
        id++;
    id++;
    return id;
}

std::string Sequence::getGlob() const
{
    return std::string(&glob[0]);
}

void Sequence::setGlob(const std::string& g)
{
    strncpy(&glob[0], &g[0], glob.capacity());
}

void Sequence::removeCurrentFrame()
{
    if (collection->getLength() <= 1) {
        return;
    }
    int index = player->frame - 1;
    collection = new MaskedImageCollection(uneditedCollection, index);
    uneditedCollection = collection;
    editGUI->validate(*this);
    player->reconfigureBounds();
    // TODO: handle SVG collection
}

