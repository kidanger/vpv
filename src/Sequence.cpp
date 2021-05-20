#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>

#include "Colormap.hpp"
#include "EditGUI.hpp"
#include "Histogram.hpp"
#include "Image.hpp"
#include "ImageCollection.hpp"
#include "ImageProvider.hpp"
#include "Player.hpp"
#include "SVG.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "editors.hpp"
#include "globals.hpp"
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
    uneditedCollection = nullptr;

    valid = false;

    loadedFrame = -1;
}

Sequence::~Sequence()
{
}

void Sequence::setImageCollection(std::shared_ptr<ImageCollection> new_imagecollection, const std::string& new_name)
{
    uneditedCollection = new_imagecollection;
    name = new_name;

    valid = uneditedCollection->getLength();
    loadedFrame = -1;

    // validation updates the collection member
    // and takes care of the player and forgetImage
    editGUI.validate(*this);
}

void Sequence::setSVGGlobs(const std::vector<std::string>& svgglobs)
{
    svgcollection.clear();
    for (const auto& glob : svgglobs) {
        std::vector<std::string> files;
        if (glob == "auto") {
            for (int i = 0; i < uneditedCollection->getLength(); i++) {
                std::string filename = uneditedCollection->getFilename(i);
                int h;
                for (h = filename.size() - 1; h > 0 && filename[h] != '.'; h--)
                    ;
                filename.resize(h);
                filename = filename + ".svg";
                files.push_back(filename);
            }
        } else {
            recursive_collect(files, glob);
        }
        svgcollection.push_back(files);
    }
}

const std::string& Sequence::getName() const
{
    return name;
}

int Sequence::getDesiredFrameIndex() const
{
    assert(player);
    return std::min(player->frame, collection->getLength());
}

void Sequence::tick()
{
    bool shouldShowDifferentFrame = false;
    if (player && collection && loadedFrame != getDesiredFrameIndex()) {
        shouldShowDifferentFrame = true;
    }
    if (valid && shouldShowDifferentFrame && (image || !error.empty())) {
        forgetImage();
    }

    if (imageprovider && imageprovider->isLoaded()) {
        ImageProvider::Result result = imageprovider->getResult();
        if (result.has_value()) {
            image = result.value();
            error.clear();
        } else {
            error = result.error();
            forgetImage();
        }
        gActive = std::max(gActive, 2);
        imageprovider = nullptr;
        if (image) {
            auto mode = gSmoothHistogram ? Histogram::Mode::SMOOTH : Histogram::Mode::EXACT;
            image->histogram->request(image, mode);
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
    image = nullptr;
    if (player && collection) {
        int desiredFrame = getDesiredFrameIndex();
        imageprovider = collection->getImageProvider(desiredFrame - 1);
        loadedFrame = desiredFrame;
    }
}

void Sequence::autoScaleAndBias(ImVec2 p1, ImVec2 p2, float quantile)
{
    std::shared_ptr<Image> img = getCurrentImage();
    if (!img)
        return;

    BandIndices bands = colormap->bands;
    float low = std::numeric_limits<float>::max();
    float high = std::numeric_limits<float>::lowest();
    bool norange = p1.x == p2.x && p1.y == p2.y && p1.x == 0 && p2.x == 0;

    if (!norange) {
        if (p1.x < 0)
            p1.x = 0;
        if (p1.y < 0)
            p1.y = 0;
        if (p2.x < 0)
            p2.x = 0;
        if (p2.y < 0)
            p2.y = 0;
        if (p1.x >= img->w - 1)
            p1.x = img->w - 1;
        if (p1.y >= img->h - 1)
            p1.y = img->h - 1;
        if (p2.x >= img->w)
            p2.x = img->w;
        if (p2.y >= img->h)
            p2.y = img->h;
        if (p1.x == p2.x)
            return;
        if (p1.y == p2.y)
            return;
    }

    if (quantile == 0) {
        if (norange) {
            low = img->min;
            high = img->max;
        } else {
            const float* data = (const float*)img->pixels;
            for (int d = 0; d < 3; d++) {
                int b = bands[d];
                if (b >= img->c)
                    continue;
                for (int y = p1.y; y < p2.y; y++) {
                    for (int x = p1.x; x < p2.x; x++) {
                        float v = data[b + img->c * (x + y * img->w)];
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
        const float* data = (const float*)img->pixels;
        if (norange) {
            if (img->c <= 3 && bands == BANDS_DEFAULT) {
                // fast path
                all = std::vector<float>(data, data + img->w * img->h * img->c);
            } else {
                for (int d = 0; d < 3; d++) {
                    int b = bands[d];
                    if (b >= img->c)
                        continue;
                    for (int y = 0; y < img->h; y++) {
                        for (int x = 0; x < img->w; x++) {
                            float v = data[b + img->c * (x + y * img->w)];
                            all.push_back(v);
                        }
                    }
                }
            }
        } else {
            if (img->c <= 3 && bands == BANDS_DEFAULT) {
                // fast path
                for (int y = p1.y; y < p2.y; y++) {
                    const float* start = &data[0 + img->c * ((int)p1.x + y * img->w)];
                    const float* end = &data[0 + img->c * ((int)p2.x + y * img->w)];
                    all.insert(all.end(), start, end);
                }
            } else {
                for (int d = 0; d < 3; d++) {
                    int b = bands[d];
                    if (b >= img->c)
                        continue;
                    for (int y = p1.y; y < p2.y; y++) {
                        for (int x = p1.x; x < p2.x; x++) {
                            float v = data[b + img->c * (x + y * img->w)];
                            all.push_back(v);
                        }
                    }
                }
            }
        }
        all.erase(std::remove_if(all.begin(), all.end(),
                      [](float x) { return !std::isfinite(x); }),
            all.end());
        std::sort(all.begin(), all.end());
        low = all[quantile * all.size()];
        high = all[(1 - quantile) * all.size()];
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

    double dynamics[] = { 1., std::pow(2, 8) - 1, std::pow(2, 16) - 1, std::pow(2, 32) - 1 };
    int best = 0;

    for (int d = sizeof(dynamics) / sizeof(double) - 1; d >= 0; d--) {
        if (min > -dynamics[d] / 1.5 && max - min < dynamics[d] * 2.)
            best = d;
    }

    colormap->autoCenterAndRadius(0., dynamics[best]);
}

std::shared_ptr<Image> Sequence::getCurrentImage()
{
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
    for (const auto& seq : gSequences) {
        if (view == seq->view && seq->image && largestW < seq->image->w) {
            largestW = seq->image->w;
        }
    }
    previousFactor = (float)largestW / image->w;
    return previousFactor;
}

std::vector<std::shared_ptr<SVG>> Sequence::getCurrentSVGs() const
{
    std::vector<std::shared_ptr<SVG>> svgs;
    if (!player)
        goto end;
    for (const auto& svgfilenames : svgcollection) {
        if (svgfilenames.empty()) {
            continue;
        }
        int frame = player->frame - 1;
        if (player->frame > svgfilenames.size()) {
            frame = 0;
        }
        svgs.push_back(SVG::get(svgfilenames[frame]));
    }
    for (const auto& i : scriptSVGs) {
        svgs.push_back(i.second);
    }
end:
    return svgs;
}

const std::string Sequence::getTitle(int ncharname) const
{
    if (!valid)
        return "(the sequence '" + name + "' contains no images)";
    if (!player)
        return "(no player associated with the sequence '" + name + "')";
    if (!colormap)
        return "(no colormap associated with the sequence '" + name + "')";

    std::string title;
    int id = 0;
    while (gSequences[id] != shared_from_this() && id < gSequences.size())
        id++;
    id++;
    title += "#" + std::to_string(id) + " ";
    title += "[" + std::to_string(loadedFrame) + '/' + std::to_string(collection->getLength()) + "]";

    assert(loadedFrame);
    std::string filename(collection->getFilename(loadedFrame - 1));
    int p = filename.size() - ncharname;
    if (p < 0 || ncharname == -1)
        p = 0;
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

    if (image) {
        int i = 0;
        for (const auto& svg : getCurrentSVGs()) {
            ImGui::Text("SVG %d: %s%s", i + 1, svg->filename.c_str(), (!svg->valid ? " invalid" : ""));
            i++;
        }
        ImGui::Text("Size: %lux%lux%lu", image->w, image->h, image->c);
        ImGui::Text("Range: %g..%g", static_cast<double>(image->min), static_cast<double>(image->max));
        ImGui::Text("Zoom: %d%%", (int)(view->zoom * getViewRescaleFactor() * 100));
        ImGui::Separator();

        if (colormap->initialized) {
            float cmin, cmax;
            colormap->getRange(cmin, cmax, image->c);
            ImGui::Text("Displayed: %g..%g", static_cast<double>(cmin), static_cast<double>(cmax));
            ImGui::Text("Shader: %s", colormap->getShaderName().c_str());
        } else {
            ImGui::Text("Colormap not initialized");
        }
        if (editGUI.isEditing()) {
            ImGui::Text("Edited with %s", editGUI.getEditorName().c_str());
        }
    }
}

void Sequence::setEdit(const std::string& edit, EditType edittype)
{
    editGUI.edittype = edittype;
    editGUI.editprog = edit;
    editGUI.validate(*this);
}

const std::string& Sequence::getEdit() const
{
    return editGUI.editprog;
}

int Sequence::getId() const
{
    int id = 0;
    while (gSequences[id] != shared_from_this() && id < gSequences.size())
        id++;
    id++;
    return id;
}

void Sequence::attachView(std::shared_ptr<View> new_view)
{
    if (view) {
        view->onSequenceDetach(shared_from_this());
    }
    view = new_view;
    if (view) {
        view->onSequenceAttach(shared_from_this());
    }
}

void Sequence::attachPlayer(std::shared_ptr<Player> new_player)
{
    if (player) {
        player->onSequenceDetach(shared_from_this());
    }
    player = new_player;
    if (player) {
        player->onSequenceAttach(shared_from_this());
    }
}

void Sequence::attachColormap(std::shared_ptr<Colormap> new_colormap)
{
    if (colormap) {
        colormap->onSequenceDetach(shared_from_this());
    }
    colormap = new_colormap;
    if (colormap) {
        colormap->onSequenceAttach(shared_from_this());
    }
}

void Sequence::removeCurrentFrame()
{
    if (collection->getLength() <= 1) {
        return;
    }
    int index = player->frame - 1;
    collection = std::make_shared<MaskedImageCollection>(uneditedCollection, index);
    uneditedCollection = collection;
    editGUI.validate(*this);
    player->reconfigureBounds();
}

bool Sequence::putScriptSVG(const std::string& key, const std::string& buf)
{
    if (buf.empty()) {
        scriptSVGs.erase(key);
    } else {
        std::shared_ptr<SVG> svg = SVG::createFromString(buf);
        if (svg->valid) {
            scriptSVGs[key] = svg;
        } else {
            return false;
        }
    }
    return true;
}
