#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "imgui.h"

struct Image {
    float* pixels;
    int w, h;
    ImVec2 size;
    enum Format {
        R=1,
        RG,
        RGB,
        RGBA,
    } format;
    float min;
    float max;
    bool is_cached;
    uint64_t lastUsed;
    std::vector<std::vector<long>> histograms;
    float histmin, histmax;

    Image(float* pixels, int w, int h, Format format);
    ~Image();

    void getPixelValueAt(int x, int y, float* values, int d) const;

    void computeHistogram(float min, float max);

    //static Image* load(const std::string& filename, bool force_load=true);

    static std::unordered_map<std::string, Image*> cache;
    static void flushCache();
};

class ImageProvider;

class ImageCollection {
public:
    //virtual Image* getImage(int index) = 0;
    virtual const std::string& getFilename(int index) = 0;
    virtual int getLength() = 0;
    virtual ImageProvider* getImageProvider(int index) = 0;
};

class MultipleImageCollection : public ImageCollection {
    std::vector<ImageCollection*> collections;
    std::vector<int> lengths;
    int totalLength;

public:

    MultipleImageCollection() : totalLength(0) {
    }

    void append(ImageCollection* ic) {
        collections.push_back(ic);
        int len = ic->getLength();
        lengths.push_back(len);
        totalLength += len;
    }

    const std::string& getFilename(int index) {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getFilename(index);
    }

    //Image* getImage(int index) {
        //int i = 0;
        //while (index < totalLength && index >= lengths[i]) {
            //index -= lengths[i];
            //i++;
        //}
        //return collections[i]->getImage(index);
    //}

    int getLength() {
        return totalLength;
    }

    ImageProvider* getImageProvider(int index) {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getImageProvider(index);
    }
};

class SingleImageImageCollection : public ImageCollection {
    std::string filename;
    ImageProvider* provider;

public:

    SingleImageImageCollection(const std::string& filename);

    //Image* getImage(int index) {
        //return provider->getImage();
    //}

    const std::string& getFilename(int index) {
        return filename;
    }

    int getLength() {
        return 1;
    }

    ImageProvider* getImageProvider(int index) {
        return provider;
    }
};

