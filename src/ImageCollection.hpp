#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cassert>

#include "Image.hpp"

class ImageCollection {

public:
    virtual ~ImageCollection() {
    }
    virtual int getLength() const = 0;
    virtual std::shared_ptr<Image> getImage(int index) const = 0;
    virtual const std::string& getFilename(int index) const = 0;
    virtual void onFileReload(const std::string& filename) = 0;
};

ImageCollection* buildImageCollectionFromFilenames(std::vector<std::string>& filenames);

class MultipleImageCollection : public ImageCollection {
    std::vector<ImageCollection*> collections;
    std::vector<int> lengths;
    int totalLength;

public:

    MultipleImageCollection() : totalLength(0) {
    }

    virtual ~MultipleImageCollection() {
        for (auto c : collections) {
            delete c;
        }
        collections.clear();
    }

    void append(ImageCollection* ic) {
        collections.push_back(ic);
        int len = ic->getLength();
        lengths.push_back(len);
        totalLength += len;
    }

    const std::string& getFilename(int index) const {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getFilename(index);
    }

    int getLength() const {
        return totalLength;
    }

    std::shared_ptr<Image> getImage(int index) const {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getImage(index);
    }

    void onFileReload(const std::string& filename) {
        for (auto c : collections) {
            c->onFileReload(filename);
        }
    }
};

class SingleImageImageCollection : public ImageCollection {
    std::string filename;
    mutable std::string key;
    std::shared_ptr<Image> image;

public:

    SingleImageImageCollection(const std::string& filename) : filename(filename), image(create_image_from_filename(filename)) {
    }

    virtual ~SingleImageImageCollection() {
    }

    const std::string& getFilename(int index) const {
        return filename;
    }

    int getLength() const {
        return 1;
    }

    virtual std::shared_ptr<Image> getImage(int index) const;

    void onFileReload(const std::string& fname) {
        if (filename == fname) {
        }
    }
};

class VideoImageCollection : public ImageCollection {
protected:
    std::string filename;

public:

    VideoImageCollection(const std::string& filename) : filename(filename) {
    }

    virtual ~VideoImageCollection() {
    }

    const std::string& getFilename(int index) const {
        return filename;
    }

    virtual int getLength() const = 0;

    virtual std::shared_ptr<Image> getImage(int index) const = 0;

    void onFileReload(const std::string& fname) {
        if (filename == fname) {
        }
    }
};

#include "editors.hpp"
class EditedImageCollection : public ImageCollection {
    EditType edittype;
    std::string editprog;
    std::vector<ImageCollection*> collections;

public:

    EditedImageCollection(EditType edittype, const std::string& editprog,
                          const std::vector<ImageCollection*>& collections)
            : edittype(edittype), editprog(editprog), collections(collections) {
    }

    virtual ~EditedImageCollection() {
        for (auto c : collections) {
            delete c;
        }
        collections.clear();
    }

    const std::string& getFilename(int index) const {
        return collections[0]->getFilename(index);
    }

    int getLength() const {
        int length = 1;
        if (!collections.empty()) {
            length = collections[0]->getLength();
            for (auto c : collections) {
                length = std::max(length, c->getLength());
            }
        }
        return length;
    }

    std::shared_ptr<Image> getImage(int index) const;

    void onFileReload(const std::string& filename) {
        for (auto c : collections) {
            c->onFileReload(filename);
        }
    }
};

class MaskedImageCollection : public ImageCollection {
    std::shared_ptr<ImageCollection> parent;
    int masked;

public:

    MaskedImageCollection(ImageCollection* parent, int masked)
            : parent(parent), masked(masked) {
    }

    virtual ~MaskedImageCollection() {
    }

    const std::string& getFilename(int index) const {
        if (index >= masked)
            index++;
        return parent->getFilename(index);
    }

    int getLength() const {
        return parent->getLength() - 1;
    }

    std::shared_ptr<Image> getImage(int index) const {
        if (index >= masked)
            index++;
        return parent->getImage(index);
    }

    void onFileReload(const std::string& filename) {
        parent->onFileReload(filename);
    }
};

