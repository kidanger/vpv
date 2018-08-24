#pragma once

#include <iostream>
#include <thread>

#include <cassert>
#include <string>
#include <vector>
#include <memory>

#include "expected.hpp"

#if 0
#define LOG(x) \
    std::cout << std::hex << std::this_thread::get_id() << " " << this << "=" << std::string(typeid(*this).name()).substr(2, 10) << "\t" << x << std::endl;

#define LOG2(x) \
    std::cout << std::hex << std::this_thread::get_id() << " " << __func__ << "\t" << x << std::endl;
#else
#define LOG(x)
#define LOG2(x)
#endif

struct Image;

#include "Image.hpp"
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
        if (res.has_value()) {
            LOG("onFinish image=" << res.value());
        } else {
            LOG("onFinish error=" << res.error());
        }
    }

    static Result makeError(typename Result::error_type e) {
        return nonstd::make_unexpected<typename Result::error_type>(std::move(e));
    }

    void setKey(const std::string& k) {
        key = k;
        LOG("is " << key)
    }

public:
    ImageProvider() : loaded(false) {
        LOG("create provider")
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

    void mark(const std::shared_ptr<Image>& image) {
        LOG("marking an image " << image << " with key " << getKey())
        image->usedBy.insert(getKey());
    }

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
            puts(0);
            exit(0);
        } else {
            provider->progress();
            if (provider->isLoaded()) {
                Result result = provider->getResult();
                if (result.has_value()) {
                    std::shared_ptr<Image> image = result.value();
                    ImageCache::store(getKey(), image);
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


