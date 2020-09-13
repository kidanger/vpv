#include <sys/stat.h>
#include "Sequence.hpp"
#include "globals.hpp"
#include "watcher.hpp"
#include "Player.hpp"
#include "ImageCollection.hpp"

#ifdef USE_GDAL
#include <gdal.h>
#endif

std::shared_ptr<Image> SingleImageImageCollection::getImage(int index) const
{
    return image;
}

std::shared_ptr<Image> EditedImageCollection::getImage(int index) const
{
    //std::vector<std::shared_ptr<Image>> images;
    //std::shared_ptr<Image> image = edit_images(edittype, editprog, images, error);
    return nullptr; // TODO
}

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

    std::shared_ptr<Image> getImage(int index) const {
        return nullptr; // TODO
    }
};

extern "C" {
#include "npy.h"
}

class NumpyVideoImageCollection : public VideoImageCollection {
    size_t length;
    int w, h, d;
    struct npy_info ni;

    void loadHeader() {
        FILE* file = fopen(filename.c_str(), "r");
        if (!npy_read_header(file, &ni)) {
            fprintf(stderr, "[npy] error while loading header\n");
            //exit(1);
        }
        fclose(file);

        if (ni.fortran_order) {
            fprintf(stderr, "numpy array '%s' is fortran order, please ask kidanger for support.\n",
                    filename.c_str());
            exit(1);
        }

        d = 1;
        length = 1;
        if (ni.ndims == 2) {
            h = ni.dims[0];
            w = ni.dims[1];
        } else if (ni.ndims == 3) {
            if (ni.dims[2] < ni.dims[0] && ni.dims[2] < ni.dims[1]) {
                h = ni.dims[0];
                w = ni.dims[1];
                d = ni.dims[2];
            } else {
                length = ni.dims[0];
                h = ni.dims[1];
                w = ni.dims[2];
            }
        } else if (ni.ndims == 4) {
            length = ni.dims[0];
            h = ni.dims[1];
            w = ni.dims[2];
            d = ni.dims[3];
        }

        printf("opened numpy array '%s', assuming size: (n=%lu, h=%d, w=%d, d=%d), type=%s\n",
               filename.c_str(), length, h, w, d, ni.desc);
    }

public:
    NumpyVideoImageCollection(const std::string& filename) : VideoImageCollection(filename), length(0) {
        loadHeader();
    }

    ~NumpyVideoImageCollection() {
    }

    int getLength() const {
        return length;
    }

    std::shared_ptr<Image> getImage(int index) const {
        return nullptr; // TODO
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
    } else if (tag[0] == 0x93 && tag[1] == 'N' && tag[2] == 'U' && tag[3] == 'M') {
        return new NumpyVideoImageCollection(filename);
    }

end:
    return new SingleImageImageCollection(filename);
}


bool endswith(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

ImageCollection* buildImageCollectionFromFilenames(std::vector<std::string>& filenames)
{
    if (filenames.size() == 1) {
        return selectCollection(filenames[0]);
    }

    //!\  here we assume that a sequence composed of multiple files means that each file contains only one image (not true for video files)
    // the reason is just that it would be slow to check the tag of each file
    MultipleImageCollection* collection = new MultipleImageCollection();
    for (auto& f : filenames) {
        if (endswith(f, ".npy")) {  // TODO: this is ugly, but faster than checking the tag
            collection->append(new NumpyVideoImageCollection(f));
        } else {
            collection->append(new SingleImageImageCollection(f));
        }
    }
    return collection;
}

