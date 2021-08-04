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
#include "ImageRegistry.hpp"
#include "Player.hpp"
#include "SVG.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "editors.hpp"
#include "globals.hpp"
#include "shaders.hpp"

Sequence::Sequence()
    : token(getToken())
{
    static int id = 0;
    id++;
    ID = "Sequence " + std::to_string(id);

    view = nullptr;
    player = nullptr;
    colormap = nullptr;
    image = nullptr;
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
            files = buildFilenamesFromExpression(glob);
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
    if (valid && shouldShowDifferentFrame) {
        forgetImage();
    }

    auto dothing = [&](auto result) {
        if (result.has_value()) {
            image = result.value();
            error.clear();
        } else {
            error = result.error();
        }
    };

    bool gotone = false;

    auto& registry = getGlobalImageRegistry();
    auto currentKey = collection->getKey(getDesiredFrameIndex() - 1);
    if (!image && registry.getStatus(currentKey) == ImageRegistry::LOADED) {
        printf("ok from registry\n");
        ImageProvider::Result result = registry.getImage(currentKey);
        dothing(result);
        gotone = true;
    }

#if 1
    if (gotone) {
        int desiredFrame = getDesiredFrameIndex();
        // enqueue some more
        for (int i = 1; i < 20; i++) {
            int frame = (desiredFrame - 1 + i) % collection->getLength();
            pushQueue(token, collection, frame);
        }
    }
#endif

    if (image && image->hasMinMaxStats() && colormap && !colormap->initialized) {
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
    if (player && collection) {
        int desiredFrame = getDesiredFrameIndex();
        flushAndPushQueue(token, collection, desiredFrame - 1);
        loadedFrame = desiredFrame;
        image = nullptr;
    }
}

void Sequence::autoScaleAndBias(ImVec2 p1, ImVec2 p2, float quantile)
{
    std::shared_ptr<Image> img = getCurrentImage();
    if (!img)
        return;

    bool hasminmax = img->hasMinMaxStats();
    bool haschunkminmax = img->hasChunkMinMaxStats();
    bool haschunkquantile = img->hasQuantilesStats();

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
    } else {
        p1 = ImVec2(0, 0);
        p2 = ImVec2(img->w - 1, img->h - 1);
    }

#define PRECISE_QUANTILE_SIZE_THRESHOLD (1000*600)
    // if the quantiles are not yet computed, we will compute them on the fly
    // but only on a small crop
    // this crop has to be smaller than PRECISE_QUANTILE_SIZE_THRESHOLD
    // which indicates what is the maximum size of the 'on the fly' quantile computation
    bool force_online_quantile = false;
    if (!haschunkquantile) {
        size_t midx = (p1.x + p2.x) / 2;
        size_t midy = (p1.y + p2.y) / 2;
        const float midsize = 200;
        p1.x = std::max(p1.x, midx - midsize);
        p2.x = std::min(p2.x, midx + midsize);
        p1.y = std::max(p1.y, midy - midsize);
        p2.y = std::min(p2.y, midy + midsize);
        force_online_quantile = true;
    }

    const int CS = img->getStatChunkSize();
    if (quantile == 0) {
        if (norange) {
            if (hasminmax) {
                // NOTE: we might want to use the per-band stats here too
                low = img->min;
                high = img->max;
            }
        } else {
            const float* data = (const float*)img->pixels;
            for (int d = 0; d < 3; d++) {
                int b = bands[d];
                if (b >= img->c)
                    continue;

                for (size_t y = std::floor(p1.y / CS) * CS; y < std::ceil(p2.y / CS) * CS; y += CS) {
                    for (size_t x = std::floor(p1.x / CS) * CS; x < std::ceil(p2.x / CS) * CS; x += CS) {
                        if (x >= p1.x && y >= p1.y && x < p2.x - CS && y < p2.y - CS) {
                            if (haschunkminmax) {
                                float cmin = img->getChunkMin(b, x / CS, y / CS);
                                float cmax = img->getChunkMax(b, x / CS, y / CS);
                                low = std::min(low, cmin);
                                high = std::max(high, cmax);
                            }
                        } else {
                            for (size_t yy = std::max(y, (size_t)p1.y); yy < std::min(y + CS, (size_t)p2.y); yy++) {
                                for (size_t xx = std::max(x, (size_t)p1.x); xx < std::min(x + CS, (size_t)p2.x); xx++) {
                                    float v = data[b + img->c * (xx + yy * img->w)];
                                    if (std::isfinite(v)) {
                                        low = std::min(low, v);
                                        high = std::max(high, v);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        std::vector<float> q0;
        std::vector<float> q1;
        std::vector<float> others;
        const float* data = (const float*)img->pixels;
        for (int d = 0; d < 3; d++) {
            int b = bands[d];
            if (b >= img->c)
                continue;

            for (size_t y = std::floor(p1.y / CS) * CS; y < std::ceil(p2.y / CS) * CS; y += CS) {
                for (size_t x = std::floor(p1.x / CS) * CS; x < std::ceil(p2.x / CS) * CS; x += CS) {
                    if (x >= p1.x && y >= p1.y && x < p2.x - CS && y < p2.y - CS && !force_online_quantile) {
                        if (haschunkquantile) {
                            float cq0 = img->getChunkQuantile(b, x / CS, y / CS, quantile);
                            float cq1 = img->getChunkQuantile(b, x / CS, y / CS, 1 - quantile);
                            q0.push_back(cq0);
                            q1.push_back(cq1);
                        }
                    } else {
                        if ((p2.x - p1.x) * (p2.y - p1.y) > PRECISE_QUANTILE_SIZE_THRESHOLD)
                            continue;
                        for (size_t yy = std::max(y, (size_t)p1.y); yy < std::min(y + CS, (size_t)p2.y); yy++) {
                            for (size_t xx = std::max(x, (size_t)p1.x); xx < std::min(x + CS, (size_t)p2.x); xx++) {
                                float v = data[b + img->c * (xx + yy * img->w)];
                                if (std::isfinite(v)) {
                                    others.push_back(v);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!others.empty()) {
            std::sort(others.begin(), others.end());
            q0.push_back(others[quantile * others.size()]);
            q1.push_back(others[(1 - quantile) * others.size()]);
        }
        std::sort(q0.begin(), q0.end());
        std::sort(q1.begin(), q1.end());
        if (!q0.empty() && !q1.empty()) {
            // is that even close to a good approximation?
            low = q0[quantile * q0.size()];
            high = q1[(1 - quantile) * q1.size()];
        }
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
    while (gSequences[id] != shared_from_this() && id < gSequences.size()) {
        id++;
    }
    id++;
    title += "#" + std::to_string(id) + " ";
    title += "[" + std::to_string(loadedFrame) + '/' + std::to_string(collection->getLength()) + "]";

    assert(loadedFrame);
    std::string filename(collection->getFilename(loadedFrame - 1));
    int p = filename.size() - ncharname;
    if (p < 0 || ncharname == -1) {
        p = 0;
    }
    if (p < filename.size()) {
        title += " " + filename.substr(p);
    }

    int desiredFrame = getDesiredFrameIndex();
    auto key = collection->getKey(desiredFrame - 1);
    auto& registry = getGlobalImageRegistry();
    auto status = registry.getStatus(key);
    if (status == ImageRegistry::UNKNOWN) {
        title += " [probably planned for loading]";
    } else if (status == ImageRegistry::LOADING) {
        title += " [loading]";
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
        if (image->hasMinMaxStats()) {
            ImGui::Text("Range: %g..%g", static_cast<double>(image->min), static_cast<double>(image->max));
        } else {
            ImGui::Text("Range: computing...");
        }
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

    for (auto& svgfilenames : svgcollection) {
        if (index < svgfilenames.size() && svgfilenames.size() != 1) {
            svgfilenames.erase(svgfilenames.begin() + index);
        }
    }
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
