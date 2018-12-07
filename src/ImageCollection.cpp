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
        // check whether the file can be opened with libraw or not
        if (RAWFileImageProvider::canOpen(filename)) {
            return std::make_shared<RAWFileImageProvider>(filename);
        } else {
            return std::make_shared<TIFFFileImageProvider>(filename);
        }
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

class VPPVideoImageProvider : public VideoImageProvider {
    FILE* file;
    int w, h, d;
    int curh;
    float* pixels;
public:
    VPPVideoImageProvider(const std::string& filename, int index, int w, int h, int d)
        : VideoImageProvider(filename, index),
          file(fopen(filename.c_str(), "r")), w(w), h(h), d(d), curh(0) {
        fseek(file, 4+3*sizeof(int)+w*h*d*sizeof(float)*index, SEEK_SET);
        pixels = (float*) malloc(w*h*d*sizeof(float));
    }

    ~VPPVideoImageProvider() {
        if (pixels)
            free(pixels);
        fclose(file);
    }

    float getProgressPercentage() const {
        return (float) curh / h;
    }

    void progress() {
        if (curh < h) {
            if (!fread(pixels+curh*w*d, sizeof(float), w*d, file)) {
                onFinish(makeError("error vpp"));
            }
            curh++;
        } else {
            auto image = std::make_shared<Image>(pixels, w, h, d);
            onFinish(image);
            mark(image);
            pixels = nullptr;
        }
    }
};

class VPPVideoImageCollection : public VideoImageCollection {
    size_t length;
    int error;
    int w, h, d;
public:
    VPPVideoImageCollection(const std::string& filename) : VideoImageCollection(filename), length(0) {
        FILE* file = fopen(filename.c_str(), "r");
        char tag[4];
        if (fread(tag, 1, 4, file) == 4
            && fread(&w, sizeof(int), 1, file)
            && fread(&h, sizeof(int), 1, file)
            && fread(&d, sizeof(int), 1, file)) {
            fseek(file, 0, SEEK_END);
            length = (ftell(file)-4-3*sizeof(int)) / (w*h*d*sizeof(float));
        }
        fclose(file);
    }

    ~VPPVideoImageCollection() {
    }

    int getLength() const {
        return length;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) const {
        auto provider = std::make_shared<VPPVideoImageProvider>(filename, index, w, h, d);
        return std::make_shared<CacheImageProvider>(provider);
    }
};

static ImageCollection* selectCollection(const std::string& filename)
{
    struct stat st;
    unsigned char tag[4];
    FILE* file;

    if (stat(filename.c_str(), &st) == -1 || S_ISFIFO(st.st_mode)) {
        goto end;
    }

    file = fopen(filename.c_str(), "r");
    if (!file || fread(tag, 1, 4, file) != 4) {
        if (file) fclose(file);
        goto end;
    }
    fclose(file);

    if (tag[0] == 'V' && tag[1] == 'P' && tag[2] == 'P' && tag[3] == 0) {
        return new VPPVideoImageCollection(filename);
    }

end:
    return new SingleImageImageCollection(filename);
}

ImageCollection* buildImageCollectionFromFilenames(std::vector<std::string>& filenames)
{
    MultipleImageCollection* collection = new MultipleImageCollection();
    for (auto& f : filenames) {
        collection->append(selectCollection(f));
    }
    return collection;
}


