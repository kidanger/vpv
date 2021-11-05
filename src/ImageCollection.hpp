#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

struct Image;
class ImageProvider;
#include "ImageProvider.hpp"

class ImageCollection {

public:
    virtual ~ImageCollection()
    {
    }
    virtual int getLength() const = 0;
    virtual std::shared_ptr<ImageProvider> getImageProvider(int index) = 0;
    virtual const std::string& getFilename(int index) const = 0;
    virtual std::string getKey(int index) const = 0;
    virtual void onFileReload(const std::string& filename) = 0;

    virtual bool imageIsLoaded(int index) {
        return getImageProvider(index)->isLoaded();
    }
    virtual ImageProvider::Result getResult(int index) {
        return getImageProvider(index)->getResult();
    }
};

std::shared_ptr<ImageCollection> buildImageCollectionFromFilenames(const std::vector<std::string>& filenames);

class MultipleImageCollection : public ImageCollection {
    std::vector<std::shared_ptr<ImageCollection>> collections;
    std::vector<int> lengths;
    int totalLength;

public:
    MultipleImageCollection()
        : totalLength(0)
    {
    }

    ~MultipleImageCollection() override
    {
        collections.clear();
    }

    void append(std::shared_ptr<ImageCollection> ic)
    {
        collections.push_back(ic);
        int len = ic->getLength();
        lengths.push_back(len);
        totalLength += len;
    }

    const std::string& getFilename(int index) const override
    {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getFilename(index);
    }

    std::string getKey(int index) const override
    {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getKey(index);
    }

    int getLength() const override
    {
        return totalLength;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) override
    {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getImageProvider(index);
    }

    void onFileReload(const std::string& filename) override
    {
        for (const auto& c : collections) {
            c->onFileReload(filename);
        }
    }
};

#include "ImageCache.hpp"
class SingleImageImageCollection : public ImageCollection {
    std::string filename;
    mutable std::string key;
    std::shared_ptr<ImageProvider> provider;

public:
    SingleImageImageCollection(const std::string& filename)
        : filename(filename)
    {
    }

    ~SingleImageImageCollection() override = default;

    const std::string& getFilename(int index) const override
    {
        return filename;
    }

    std::string getKey(int index) const override
    {
        return "image:" + filename;
    }

    int getLength() const override
    {
        return 1;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) override;

    void onFileReload(const std::string& fname) override
    {
        if (filename == fname) {
            //ImageCache::remove(filename);
        }
    }
};

class VideoImageCollection : public ImageCollection {
protected:
    std::string filename;

public:
    VideoImageCollection(const std::string& filename)
        : filename(filename)
    {
    }

    ~VideoImageCollection() override = default;

    const std::string& getFilename(int index) const override
    {
        return filename;
    }

    std::string getKey(int index) const override
    {
        return "video:" + filename + ":" + std::to_string(index);
    }

    int getLength() const override = 0;

    std::shared_ptr<ImageProvider> getImageProvider(int index) override = 0;

    void onFileReload(const std::string& fname) override
    {
        if (filename == fname) {
        }
    }
};

#include "editors.hpp"
class EditedImageCollection : public ImageCollection {
    EditType edittype;
    std::string editprog;
    std::vector<std::shared_ptr<ImageCollection>> collections;
    std::vector<std::shared_ptr<ImageProvider>> providers;

public:
    EditedImageCollection(EditType edittype, const std::string& editprog,
        const std::vector<std::shared_ptr<ImageCollection>>& collections)
        : edittype(edittype)
        , editprog(editprog)
        , collections(collections)
        , providers(collections.size(), nullptr)
    {
    }

    ~EditedImageCollection() override
    {
        collections.clear();
    }

    const std::string& getFilename(int index) const override
    {
        return collections[0]->getFilename(index);
    }

    std::string getKey(int index) const override
    {
        std::string key("edit:" + std::to_string(edittype) + editprog);
        for (const auto& c : collections)
            key += c->getKey(index);
        return key;
    }

    int getLength() const override
    {
        int length = 1;
        if (!collections.empty()) {
            length = collections[0]->getLength();
            for (const auto& c : collections) {
                length = std::max(length, c->getLength());
            }
        }
        return length;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) override;

    void onFileReload(const std::string& filename) override
    {
        for (const auto& c : collections) {
            c->onFileReload(filename);
        }
    }
};

class MaskedImageCollection : public ImageCollection {
    std::shared_ptr<ImageCollection> parent;
    int masked;

public:
    MaskedImageCollection(std::shared_ptr<ImageCollection> parent, int masked)
        : parent(parent)
        , masked(masked)
    {
    }

    ~MaskedImageCollection() override = default;

    const std::string& getFilename(int index) const override
    {
        if (index >= masked)
            index++;
        return parent->getFilename(index);
    }

    std::string getKey(int index) const override
    {
        if (index >= masked)
            index++;
        return parent->getKey(index);
    }

    int getLength() const override
    {
        return parent->getLength() - 1;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) override
    {
        if (index >= masked)
            index++;
        return parent->getImageProvider(index);
    }

    void onFileReload(const std::string& filename) override
    {
        parent->onFileReload(filename);
    }
};

class FixedImageCollection : public ImageCollection {
    std::shared_ptr<ImageCollection> parent;
    int index;

public:
    FixedImageCollection(std::shared_ptr<ImageCollection> parent, int index)
        : parent(parent)
        , index(index)
    {
    }

    ~FixedImageCollection() override = default;

    const std::string& getFilename(int) const override
    {
        return parent->getFilename(index);
    }

    std::string getKey(int) const override
    {
        return parent->getKey(index);
    }

    int getLength() const override
    {
        return 1;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int) override
    {
        return parent->getImageProvider(index);
    }

    void onFileReload(const std::string& filename) override
    {
        parent->onFileReload(filename);
    }
};

class OffsetedImageCollection : public ImageCollection {
    std::shared_ptr<ImageCollection> parent;
    int offset;

public:
    OffsetedImageCollection(std::shared_ptr<ImageCollection> parent, int offset)
        : parent(parent)
        , offset(offset)
    {
    }

    ~OffsetedImageCollection() override = default;

    const std::string& getFilename(int index) const override
    {
        index = std::max(0, index + offset);
        return parent->getFilename(index);
    }

    std::string getKey(int index) const override
    {
        index = std::max(0, index + offset);
        return parent->getKey(index);
    }

    int getLength() const override
    {
        return parent->getLength() - offset;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) override
    {
        index = std::max(0, index + offset);
        return parent->getImageProvider(index);
    }

    void onFileReload(const std::string& filename) override
    {
        parent->onFileReload(filename);
    }
};
