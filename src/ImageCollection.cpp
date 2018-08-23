#include "ImageProvider.hpp"
#include "ImageCollection.hpp"

std::shared_ptr<ImageProvider> selectProvider(const std::string& filename)
{
    auto idx = filename.rfind('.');
    if(idx != std::string::npos) {
        std::string extension = filename.substr(idx+1);
        if (extension == "jpg" || extension == "JPG" || extension == "jpeg" || extension == "JPEG") {
            return std::make_shared<JPEGFileImageProvider>(filename);
        }
    }
    return std::make_shared<IIOFileImageProvider>(filename);
}

std::shared_ptr<ImageProvider> SingleImageImageCollection::getImageProvider(int) const
{
    std::shared_ptr<ImageProvider> provider = selectProvider(filename);
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

