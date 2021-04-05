#pragma once

#include <iostream>
#include <thread>

#include <functional>
#include <cassert>
#include <string>
#include <vector>
#include <memory>

#include "expected.hpp"

#include "Progressable.hpp"

struct Image;

#include "Image.hpp"
class ImageProvider : public Progressable {
public:
    using Result = nonstd::expected<std::shared_ptr<Image>, std::string>;

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

    bool isLoaded() const override {
        return loaded;
    }

};

#include "ImageCache.hpp"
class CacheImageProvider : public ImageProvider {
    std::string key;
    std::function<std::shared_ptr<ImageProvider>()> get;
    std::shared_ptr<ImageProvider> provider;

public:
    CacheImageProvider(const std::string& key, const std::function<std::shared_ptr<ImageProvider>()>& get)
        : key(key), get(get) {
        if (ImageCache::has(key)) {
            onFinish(ImageCache::get(key));
        } else if (ImageCache::Error::has(key)) {
            onFinish(makeError(ImageCache::Error::get(key)));
        } else {
            provider = get();
        }
    }

    ~CacheImageProvider() override = default;

    float getProgressPercentage() const override {
        if (ImageCache::has(key)) {
            return 1.f;
        }
        return provider->getProgressPercentage();
    }

    void progress() override {
        if (ImageCache::has(key)) {
            onFinish(Result(ImageCache::get(key)));
            //printf("/!\\ inconsistent image loading\n");
        } else {
            provider->progress();
            if (provider->isLoaded()) {
                Result result = provider->getResult();
                if (result.has_value()) {
                    std::shared_ptr<Image> image = result.value();
                    ImageCache::store(key, image);
                } else {
                    ImageCache::Error::store(key, result.error());
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
    }
};

class IIOFileImageProvider : public FileImageProvider {
public:
    IIOFileImageProvider(const std::string& filename) : FileImageProvider(filename) {
    }

    ~IIOFileImageProvider() override = default;

    float getProgressPercentage() const override {
        return 0.f;
    }

    void progress() override;
};

#ifdef USE_GDAL
class GDALFileImageProvider : public FileImageProvider {
private:
    float df;
public:
    GDALFileImageProvider(const std::string& filename) : FileImageProvider(filename) {
    }

    ~GDALFileImageProvider() override = default;

    float getProgressPercentage() const override {
        return df;
    }

    void progress() override;
};
#endif

class JPEGFileImageProvider : public FileImageProvider {
private:
    class impl;
    std::unique_ptr<impl> pimpl;

public:
    JPEGFileImageProvider(const std::string& filename);

    ~JPEGFileImageProvider() override;

    float getProgressPercentage() const override;

    void progress() override;
};

class PNGFileImageProvider : public FileImageProvider {
    struct PNGPrivate* p;

    int initialize_png_reader();

public:
    PNGFileImageProvider(const std::string& filename)
        : FileImageProvider(filename), p(nullptr)
    {
    }

    ~PNGFileImageProvider() override;

    float getProgressPercentage() const override;

    void progress() override;

    void onPNGError(const std::string& error);
};

class TIFFFileImageProvider : public FileImageProvider {
    struct TIFFPrivate* p;

public:
    TIFFFileImageProvider(const std::string& filename)
        : FileImageProvider(filename), p(nullptr)
    {
    }

    ~TIFFFileImageProvider() override;

    float getProgressPercentage() const override;

    void progress() override;
};

class RAWFileImageProvider : public FileImageProvider {

public:
    RAWFileImageProvider(const std::string& filename)
        : FileImageProvider(filename)
    {
    }

    ~RAWFileImageProvider() override;

    float getProgressPercentage() const override;

    void progress() override;

    static bool canOpen(const std::string& filename);
};

#include "editors.hpp"
class EditedImageProvider : public ImageProvider {
    EditType edittype;
    std::string editprog;
    std::vector<std::shared_ptr<ImageProvider>> providers;
    std::string key; // used for usedBy

public:
    EditedImageProvider(EditType edittype, const std::string& editprog,
                        const std::vector<std::shared_ptr<ImageProvider>>& providers,
                        const std::string& key)
        : edittype(edittype), editprog(editprog), providers(providers), key(key)
    {
    }

    ~EditedImageProvider() override {
        providers.clear();
    }

    float getProgressPercentage() const override {
        float percent = 0.f;
        for (const auto& p : providers) {
            percent += p->getProgressPercentage();
        }
        percent /= (providers.size() + 1); // +1 because of the edition time
        return percent;
    }

    void progress() override;
};

class VideoImageProvider : public ImageProvider {
protected:
    std::string filename;
    int frame;
public:
    VideoImageProvider(const std::string& filename, int frame) : filename(filename), frame(frame) {
    }
};

