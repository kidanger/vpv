#include "ImageProvider.hpp"
#include "ImageCollection.hpp"

std::shared_ptr<ImageProvider> SingleImageImageCollection::getImageProvider(int index) const
{
    std::shared_ptr<ImageProvider> provider = std::make_shared<IIOFileImageProvider>(filename);
    return std::make_shared<CacheImageProvider>(provider, filename);
}

std::shared_ptr<ImageProvider> EditedImageCollection::getImageProvider(int index) const
{
    std::vector<std::shared_ptr<ImageProvider>> providers;
    for (auto c : collections) {
        providers.push_back(c->getImageProvider(index));
    }
    std::string key(editprog+std::to_string(index)); // FIXME
    return std::make_shared<CacheImageProvider>(std::make_shared<EditedImageProvider>(edittype, editprog, providers), key);
}

