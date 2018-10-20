#include <sys/stat.h>
#include "ImageProvider.hpp"
#include "Sequence.hpp"
#include "globals.hpp"
#include "watcher.hpp"
#include "ImageCollection.hpp"

static std::shared_ptr<ImageProvider> selectProvider(const std::string& filename)
{
    struct stat st;
    unsigned char tag[4];
    FILE* file;

    if (stat(filename.c_str(), &st) == -1 || S_ISFIFO(st.st_mode)) {
        // -1 can append because we use "-" to indicate stdin
        // all fifos are handled by iio
        goto iio;
    }

    file = fopen(filename.c_str(), "r");
    if (!file || fread(tag, 1, 4, file) != 4) {
        if (file) fclose(file);
        goto iio;
    }
    fclose(file);

    if (tag[0]==0xff && tag[1]==0xd8 && tag[2]==0xff) {
        return std::make_shared<JPEGFileImageProvider>(filename);
    } else if (tag[1]=='P' && tag[2]=='N' && tag[3]=='G') {
        return std::make_shared<PNGFileImageProvider>(filename);
    } else if ((tag[0]=='M' && tag[1]=='M') || (tag[0]=='I' && tag[1]=='I')) {
        return std::make_shared<TIFFFileImageProvider>(filename);
    }
iio:
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

