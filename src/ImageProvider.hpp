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

protected:
    void onFinish(const Result& res) {
        this->result = res;
        loaded = true;
    }

    static Result makeError(typename Result::error_type e) {
        return nonstd::make_unexpected<typename Result::error_type>(std::move(e));
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

    virtual float getProgressPercentage() const = 0;
    virtual void progress() = 0;
};

#include "ImageCache.hpp"
class CacheImageProvider : public ImageProvider {
    std::shared_ptr<ImageProvider> provider;
    std::string key;

public:
    CacheImageProvider(const std::shared_ptr<ImageProvider>& provider, const std::string& key)
        : provider(provider), key(key) {
        if (ImageCache::has(key)) {
            onFinish(ImageCache::get(key));
        } else {
            if (ImageCache::Error::has(key)) {
                onFinish(makeError(ImageCache::Error::get(key)));
            }
        }
    }

    virtual ~CacheImageProvider() {
    }

    virtual float getProgressPercentage() const {
        if (ImageCache::has(key)) {
            return 1.f;
        }
        return provider->getProgressPercentage();
    }

    virtual void progress() {
        if (ImageCache::has(key)) {
            onFinish(Result(ImageCache::get(key)));
        } else {
            provider->progress();
            if (provider->isLoaded()) {
                Result result = provider->getResult();
                if (result.has_value()) {
                    ImageCache::store(key, result.value());
                    printf("%s put to cache\n", key.c_str());
                } else {
                    ImageCache::Error::store(key, result.error());
                }
                onFinish(result);
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

    virtual ~IIOFileImageProvider() {
    }

    virtual float getProgressPercentage() const {
        return 0.f;
    }

    virtual void progress();
};

class JPEGFileImageProvider : public FileImageProvider {
    std::string filename;
    struct jpeg_decompress_struct* cinfo;
    FILE* file;
    float* pixels;
    unsigned char* scanline;
    bool error;
    struct jpeg_error_mgr* jerr;

public:
    JPEGFileImageProvider(const std::string& filename)
        : filename(filename), cinfo(nullptr), file(nullptr), error(false), pixels(nullptr), scanline(nullptr), jerr(nullptr)
    {
    }

    virtual ~JPEGFileImageProvider();

    virtual float getProgressPercentage() const;

    virtual void progress();

    void onJPEGError(const std::string& error);

};

class PNGFileImageProvider : public FileImageProvider {
    std::string filename;
    struct PNGPrivate* p;

    int initialize_png_reader();
    int process_data(char* buffer, uint32_t length);

public:
    PNGFileImageProvider(const std::string& filename)
        : filename(filename), p(nullptr)
    {
    }

    virtual ~PNGFileImageProvider();

    virtual float getProgressPercentage() const;

    virtual void progress();

    void info_callback();
    void row_callback(unsigned char* new_row, uint32_t row_num, int pass);
    void end_callback();
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


