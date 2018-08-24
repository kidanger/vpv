#include "ImageProvider.hpp"
#include "Sequence.hpp"
#include "globals.hpp"
#include "watcher.hpp"
#include "ImageCollection.hpp"

static std::shared_ptr<ImageProvider> selectProvider(const std::string& filename)
{
    auto idx = filename.rfind('.');
    if(idx != std::string::npos) {
        std::string extension = filename.substr(idx+1);
        if (extension == "jpg" || extension == "JPG" || extension == "jpeg" || extension == "JPEG") {
            return std::make_shared<JPEGFileImageProvider>(filename);
        } else if (extension == "png" || extension == "PNG") {
            return std::make_shared<PNGFileImageProvider>(filename);
        }
    }

    return std::make_shared<IIOFileImageProvider>(filename);
}

std::shared_ptr<ImageProvider> SingleImageImageCollection::getImageProvider(int) const
{
    std::shared_ptr<ImageProvider> provider = selectProvider(filename);
    if (key.empty()) {
        key = provider->getKey();
        watcher_add_file(filename, [this](const std::string& fname) {
            LOG("file changed " << filename);
            ImageCache::Error::remove(key);
            ImageCache::remove(key);
            for (auto seq : gSequences) {
                seq->forgetImage();
            }
        });
    }
    return std::make_shared<CacheImageProvider>(provider);
}

std::shared_ptr<ImageProvider> EditedImageCollection::getImageProvider(int index) const
{
    std::vector<std::shared_ptr<ImageProvider>> providers;
    for (auto c : collections) {
        providers.push_back(c->getImageProvider(index));
    }
    std::shared_ptr<ImageProvider> provider = std::make_shared<EditedImageProvider>(edittype, editprog, providers);
    return std::make_shared<CacheImageProvider>(provider);
}

