#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cassert>

struct Image;
class ImageProvider;

class ImageCollection {

public:
    virtual ~ImageCollection() {
    }
    virtual int getLength() const = 0;
    virtual std::shared_ptr<ImageProvider> getImageProvider(int index) const = 0;
    virtual const std::string& getFilename(int index) const = 0;
    virtual void onFileReload(const std::string& filename) = 0;
};

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
        if (collections.empty()) puts(0);
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

    std::shared_ptr<ImageProvider> getImageProvider(int index) const {
        if (collections.empty()) puts(0);
        int i = 0;
        while (index < totalLength && index >= lengths[i]) {
            index -= lengths[i];
            i++;
        }
        return collections[i]->getImageProvider(index);
    }

    void onFileReload(const std::string& filename) {
        for (auto c : collections) {
            c->onFileReload(filename);
        }
    }
};

#include "ImageCache.hpp"
class SingleImageImageCollection : public ImageCollection {
    std::string filename;
    mutable std::string key;

public:

    SingleImageImageCollection(const std::string& filename) : filename(filename) {
    }

    virtual ~SingleImageImageCollection() {
    }

    const std::string& getFilename(int index) const {
        return filename;
    }

    int getLength() const {
        return 1;
    }

    virtual std::shared_ptr<ImageProvider> getImageProvider(int index) const;

    void onFileReload(const std::string& fname) {
        if (filename == fname) {
            //ImageCache::remove(filename);
        }
    }
};

#include "editors.hpp"
class EditedImageCollection : public ImageCollection {
    EditType edittype;
    std::string editprog;
    std::vector<ImageCollection*> collections;
    int length;

public:

    EditedImageCollection(EditType edittype, const std::string& editprog,
                          const std::vector<ImageCollection*>& collections)
            : edittype(edittype), editprog(editprog), collections(collections), length(1) {
        if (!collections.empty()) {
            length = collections[0]->getLength();
            for (auto c : collections) {
                length = std::min(length, c->getLength());
            }
        }
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
        return length;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) const;

    void onFileReload(const std::string& filename) {
        for (auto c : collections) {
            c->onFileReload(filename);
        }
    }
};


