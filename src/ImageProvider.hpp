#pragma once

#include <cassert>
#include <string>

struct Image;

class ImageProvider {
private:
    Image* image;
    bool loaded;

protected:
    void onFinish(Image* image) {
        this->image = image;
        loaded = true;
    }

public:

    ImageProvider() : loaded(false) {
    }

    Image* getImage() {
        if (!loaded) return nullptr;
        assert(loaded);
        Image* img = this->image;
        this->image = nullptr;
        loaded = false;
        return img;
    }

    bool isLoaded() {
        return loaded;
    }

    virtual float getProgressPercentage() = 0;
    virtual void progress() = 0;
};

#include "ImageCache.hpp"
class CacheImageProvider : public ImageProvider {
    ImageProvider* provider;
    std::string key;

public:
    CacheImageProvider(ImageProvider* provider, const std::string& key) : provider(provider), key(key) {
    }

    virtual float getProgressPercentage() {
        if (ImageCache::has(key)) {
            return 1.f;
        }
        return provider->getProgressPercentage();
    }

    virtual void progress() {
        if (ImageCache::has(key)) {
            onFinish(ImageCache::get(key));
        } else {
            provider->progress();
            if (provider->isLoaded()) {
                Image* image = provider->getImage();
                if (image) {
                    ImageCache::store(key, image);
                }
                onFinish(image);
            }
        }
    }
};

class FileImageProvider : public ImageProvider {
public:
};

#include "Image.hpp"
extern "C" {
#include "iio.h"
}
class IIOFileImageProvider : public FileImageProvider {
    std::string filename;

public:
    IIOFileImageProvider(const std::string& filename) : filename(filename) {
    }

    virtual float getProgressPercentage() {
        return 0.f;
    }

    virtual void progress() {
        int w, h, d;
        printf("loading '%s'\n", filename.c_str());
        float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
        if (!pixels) {
            fprintf(stderr, "cannot load image '%s'\n", filename.c_str());
        }

        if (d > 4) {
            printf("warning: '%s' has %d channels, extracting the first four\n", filename.c_str(), d);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    for (int l = 0; l < 4; l++) {
                        pixels[(y*h+x)*4+l] = pixels[(y*h+x)*d+l];
                    }
                }
            }
            d = 4;
        }
        Image* image = new Image(pixels, w, h, (Image::Format) d);
        onFinish(image);
    }
};

