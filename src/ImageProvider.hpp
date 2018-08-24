#pragma once

#include <cassert>
#include <string>
#include <vector>
#include <memory>

#include "expected.hpp"

struct Image;

class ImageProvider {
public:
    typedef nonstd::expected<std::shared_ptr<Image>, std::string> Result;

private:
    bool loaded;
    Result result;
    std::string key;

protected:
    void onFinish(const Result& res) {
        this->result = res;
        loaded = true;
    }

    static Result makeError(typename Result::error_type e) {
        return nonstd::make_unexpected<typename Result::error_type>(std::move(e));
    }

    void setKey(const std::string& k) {
        key = k;
    }

public:
    ImageProvider() : loaded(false) {
    }

    virtual ~ImageProvider() {
    }

    Result getResult() {
        assert(loaded);
        return result;
    }

    bool isLoaded() {
        return loaded;
    }

    const std::string& getKey() const { return key; };

    virtual float getProgressPercentage() const = 0;
    virtual void progress() = 0;
};

#include "ImageCache.hpp"
class CacheImageProvider : public ImageProvider {
    std::shared_ptr<ImageProvider> provider;

public:
    CacheImageProvider(const std::shared_ptr<ImageProvider>& provider)
        : provider(provider) {
        setKey(provider->getKey());
        if (ImageCache::has(getKey())) {
            onFinish(ImageCache::get(getKey()));
        } else {
            if (ImageCache::Error::has(getKey())) {
                onFinish(makeError(ImageCache::Error::get(getKey())));
            }
        }
    }

    virtual ~CacheImageProvider() {
    }

    virtual float getProgressPercentage() const {
        if (ImageCache::has(getKey())) {
            return 1.f;
        }
        return provider->getProgressPercentage();
    }

    virtual void progress() {
        if (ImageCache::has(getKey())) {
            onFinish(Result(ImageCache::get(getKey())));
        } else {
            provider->progress();
            if (provider->isLoaded()) {
                Result result = provider->getResult();
                if (result.has_value()) {
                    ImageCache::store(getKey(), result.value());
                    printf("%s put to cache\n", getKey().c_str());
                } else {
                    ImageCache::Error::store(getKey(), result.error());
                }
                onFinish(result);
            }
        }
    }
};

class FileImageProvider : public ImageProvider {
protected:
    std::string filename;
public:
    FileImageProvider(const std::string& filename) : filename(filename) {
        setKey("image:" + filename);
    }
};

class IIOFileImageProvider : public FileImageProvider {
public:
    IIOFileImageProvider(const std::string& filename) : FileImageProvider(filename) {
    }

    virtual ~IIOFileImageProvider() {
    }

    virtual float getProgressPercentage() const {
        return 0.f;
    }

    virtual void progress();
};

class JPEGFileImageProvider : public FileImageProvider {
    struct jpeg_decompress_struct* cinfo;
    FILE* file;
    float* pixels;
    unsigned char* scanline;
    bool error;
    struct jpeg_error_mgr* jerr;

public:
    JPEGFileImageProvider(const std::string& filename)
        : FileImageProvider(filename), cinfo(nullptr), file(nullptr),
          pixels(nullptr), scanline(nullptr), error(false), jerr(nullptr)
    {
    }

    virtual ~JPEGFileImageProvider();

    virtual float getProgressPercentage() const;

    virtual void progress();

    void onJPEGError(const std::string& error);

};

class PNGFileImageProvider : public FileImageProvider {
    struct PNGPrivate* p;

    int initialize_png_reader();

public:
    PNGFileImageProvider(const std::string& filename)
        : FileImageProvider(filename), p(nullptr)
    {
    }

    virtual ~PNGFileImageProvider();

    virtual float getProgressPercentage() const;

    virtual void progress();

    void onPNGError(const std::string& error);
};

#include "editors.hpp"
class EditedImageProvider : public ImageProvider {
    EditType edittype;
    std::string editprog;
    std::vector<std::shared_ptr<ImageProvider>> providers;

public:
    EditedImageProvider(EditType edittype, const std::string& editprog,
                        const std::vector<std::shared_ptr<ImageProvider>>& providers)
        : edittype(edittype), editprog(editprog), providers(providers) {
        std::string key("edit:" + std::to_string(edittype) + editprog);
        for (auto p : providers)
            key += p->getKey();
        setKey(key);
    }

    virtual ~EditedImageProvider() {
        providers.clear();
    }

    virtual float getProgressPercentage() const {
        float percent = 0.f;
        for (auto p : providers) {
            percent += p->getProgressPercentage();
        }
        percent /= (providers.size() + 1); // +1 because of the edition time
        return percent;
    }

    virtual void progress();
};


