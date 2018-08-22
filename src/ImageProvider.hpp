#pragma once

#include <cassert>
#include <string>
#include <vector>

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

class IIOFileImageProvider : public FileImageProvider {
    std::string filename;

public:
    IIOFileImageProvider(const std::string& filename) : filename(filename) {
    }

    virtual float getProgressPercentage() {
        return 0.f;
    }

    virtual void progress();
};

#include "editors.hpp"
class EditedImageProvider : public ImageProvider {
    EditType edittype;
    std::string editprog;
    std::vector<ImageProvider*> providers;

public:
    EditedImageProvider(EditType edittype, const std::string& editprog,
                        const std::vector<ImageProvider*>& providers)
        : edittype(edittype), editprog(editprog), providers(providers) {
    }

    virtual float getProgressPercentage() {
        float percent = 0.f;
        for (auto p : providers) {
            percent += p->getProgressPercentage();
        }
        percent /= (providers.size() + 1); // +1 because of the edition time
        return percent;
    }

    virtual void progress();
};


