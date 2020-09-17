#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cassert>

#include "Image.hpp"

class ImageCollection {
public:
    ImageCollection() {
    }

    virtual ~ImageCollection() {
    }
    virtual int getLength() const = 0;
    virtual std::shared_ptr<Image> getImage(int index) const = 0;
    virtual std::string getFilename(int index) const = 0;
    virtual bool onFileReload(const std::string& filename) = 0;
    virtual void prepare(int index) = 0;
};

std::shared_ptr<ImageCollection> buildImageCollectionFromFilenames(std::vector<std::string>& filenames);

class MultipleImageCollection : public ImageCollection {
    std::vector<std::shared_ptr<ImageCollection>> collections;
    std::vector<int> lengths;
    int totalLength;

public:

    MultipleImageCollection() : totalLength(0) {
    }

    virtual ~MultipleImageCollection() {
    }

    void append(std::shared_ptr<ImageCollection> ic) {
        collections.push_back(ic);
        int len = ic->getLength();
        lengths.push_back(len);
        totalLength += len;
    }

    std::string getFilename(int index) const {
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

    bool onFileReload(const std::string& filename) {
        bool r = false;
        for (auto c : collections) {
            r |= c->onFileReload(filename);
        }
        return r;
    }

    virtual void prepare(int index) {
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        collections[i]->prepare(index);
    }
};

class SingleImageImageCollection : public ImageCollection {
    std::string filename;
    mutable std::string key;
    std::shared_ptr<Image> image;

public:

    SingleImageImageCollection(const std::string& filename) : filename(filename) {
    }

    virtual ~SingleImageImageCollection() {
    }

    std::string getFilename(int index) const {
        return filename;
    }

    int getLength() const {
        return 1;
    }

    virtual std::shared_ptr<Image> getImage(int index) const {
        return image;
    }

    bool onFileReload(const std::string& fname) {
        if (filename == fname) {
            image = nullptr;
            return true;
        }
        return false;
    }

    virtual void prepare(int index);
};

class VideoImageCollection : public ImageCollection {
protected:
    std::string filename;

public:

    VideoImageCollection(const std::string& filename) : filename(filename) {
    }

    virtual ~VideoImageCollection() {
    }

    std::string getFilename(int index) const {
        return filename;
    }

    virtual int getLength() const = 0;

    virtual std::shared_ptr<Image> getImage(int index) const = 0;

    bool onFileReload(const std::string& fname) {
        if (filename == fname) {
        }
        return false;
    }

    virtual void prepare(int index) = 0;
};

#include "editors.hpp"
class EditedImageCollection : public ImageCollection {
    EditType edittype;
    std::string editprog;
    std::vector<std::shared_ptr<ImageCollection>> collections;
    std::vector<std::shared_ptr<Image>> images;

public:

    EditedImageCollection(EditType edittype, const std::string& editprog,
                          const std::vector<std::shared_ptr<ImageCollection>>& collections);

    virtual ~EditedImageCollection() {
    }

    std::string getFilename(int index) const {
        // TODO: better filename generation
        return collections[0]->getFilename(index) + " (edited)";
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

    bool onFileReload(const std::string& filename) {
        bool r = false;
        for (auto c : collections) {
            r |= c->onFileReload(filename);
        }
        if (r) {
            images.clear();
            images.resize(getLength());
        }
        return r;
    }

    virtual void prepare(int index);
};

class MaskedImageCollection : public ImageCollection {
    std::shared_ptr<ImageCollection> parent;
    int masked;

public:

    MaskedImageCollection(std::shared_ptr<ImageCollection> parent, int masked)
            : parent(parent), masked(masked) {
    }

    virtual ~MaskedImageCollection() {
    }

    std::string getFilename(int index) const {
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

    bool onFileReload(const std::string& filename) {
        return parent->onFileReload(filename);
    }

    virtual void prepare(int index) {
        if (index >= masked)
            index++;
        parent->prepare(index);
    }
};

