
#include "ImageProvider.hpp"
#include "ImageCollection.hpp"

SingleImageImageCollection::SingleImageImageCollection(const std::string& filename) : filename(filename) {
    ImageProvider* provider = new IIOFileImageProvider(filename);
    this->provider = new CacheImageProvider(provider, filename);
}

ImageProvider* EditedImageCollection::getImageProvider(int index) const {
    std::vector<ImageProvider*> providers;
    for (auto c : collections) {
        providers.push_back(c->getImageProvider(index));
    }
    return new CacheImageProvider(new EditedImageProvider(edittype, editprog, providers),
                                  editprog+std::to_string(index));
}

