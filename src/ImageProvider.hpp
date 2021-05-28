#pragma once

#include <iostream>
#include <thread>

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "expected.hpp"

#include "Progressable.hpp"

struct Image;

#include "Image.hpp"
class ImageProvider : public Progressable {
public:
    using Result = nonstd::expected<std::shared_ptr<Image>, std::string>;

private:
    std::atomic<bool> loaded;
    Result result;

protected:
    void onFinish(const Result& res)
    {
        this->result = res;
        loaded = true;
    }

    static Result makeError(typename Result::error_type e)
    {
        return nonstd::make_unexpected<typename Result::error_type>(std::move(e));
    }

public:
    ImageProvider()
        : loaded(false)
    {
    }

    virtual ~ImageProvider()
    {
    }

    Result getResult()
    {
        assert(loaded);
        return result;
    }

    bool isLoaded() const override
    {
        return loaded;
    }
};

class FileImageProvider : public ImageProvider {
protected:
    std::string filename;

public:
    FileImageProvider(const std::string& filename)
        : filename(filename)
    {
    }
};

class IIOFileImageProvider : public FileImageProvider {
public:
    IIOFileImageProvider(const std::string& filename)
        : FileImageProvider(filename)
    {
    }

    ~IIOFileImageProvider() override = default;

    float getProgressPercentage() const override
    {
        return 0.f;
    }

    void progress() override;
};

#ifdef USE_GDAL
class GDALFileImageProvider : public FileImageProvider {
private:
    float df;

public:
    GDALFileImageProvider(const std::string& filename)
        : FileImageProvider(filename)
    {
    }

    ~GDALFileImageProvider() override = default;

    float getProgressPercentage() const override
    {
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
        : FileImageProvider(filename)
        , p(nullptr)
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
        : FileImageProvider(filename)
        , p(nullptr)
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

public:
    EditedImageProvider(EditType edittype, const std::string& editprog,
        const std::vector<std::shared_ptr<ImageProvider>>& providers)
        : edittype(edittype)
        , editprog(editprog)
        , providers(providers)
    {
    }

    ~EditedImageProvider() override
    {
        providers.clear();
    }

    float getProgressPercentage() const override
    {
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
    VideoImageProvider(const std::string& filename, int frame)
        : filename(filename)
        , frame(frame)
    {
    }
};
