#include <cerrno>
#include <memory>
#include <system_error>

#include "ImageCollection.hpp"
#include "ImageProvider.hpp"
#include "Player.hpp"
#include "Sequence.hpp"
#include "expected.hpp"
#include "fs.hpp"
#include "globals.hpp"
#include "strutils.hpp"
#include "watcher.hpp"

#ifdef USE_GDAL
#include <gdal.h>
#endif

static std::error_code error_code_from_errno(int errno_code)
{
    return std::make_error_code(static_cast<std::errc>(errno_code));
}

static nonstd::expected<std::array<unsigned char, 4>, std::error_code> getFileTag(const fs::path& path)
{
    std::array<unsigned char, 4> tag;
    fs::ifstream ifs(path, fs::ifstream::in | fs::ifstream::binary);

    if (!ifs || !ifs.read(reinterpret_cast<char*>(tag.data()), tag.size())) {
        return nonstd::make_unexpected(error_code_from_errno(errno));
    }

    return tag;
}

static std::shared_ptr<ImageProvider> selectProvider(const std::string& filename)
{
    static int gdalinit = (GDALAllRegister(), 1);
    // avoid blocking for now
    return std::make_shared<GDALFileImageProvider>(filename);

    if (gForceIioOpen)
        goto iio2;

    if (fs::is_fifo(fs::path(filename))) {
        // -1 can append because we use "-" to indicate stdin
        // all fifos are handled by iio
        // or because it's not a file by a virtual file system path for GDAL
        goto iio;
    }
    {
        auto result = getFileTag(filename);
        if (result) {
            auto tag = *result;
            if (tag[0] == 0xff && tag[1] == 0xd8 && tag[2] == 0xff) {
                return std::make_shared<JPEGFileImageProvider>(filename);
            } else if (tag[1] == 'P' && tag[2] == 'N' && tag[3] == 'G') {
                return std::make_shared<PNGFileImageProvider>(filename);
            } else if ((tag[0] == 'M' && tag[1] == 'M') || (tag[0] == 'I' && tag[1] == 'I')) {
                // check whether the file can be opened with libraw or not
                if (RAWFileImageProvider::canOpen(filename)) {
                    return std::make_shared<RAWFileImageProvider>(filename);
                } else {
#ifndef USE_GDAL // in case we have gdal, just use it, it's better than our loader anyway
                    return std::make_shared<TIFFFileImageProvider>(filename);
#endif
                }
            }
        }
    }
iio :
#ifdef USE_GDAL
{
    static int gdalinit = (GDALAllRegister(), 1);
    (void)gdalinit;
    // use OpenEX because Open outputs error messages to stderr
    GDALDatasetH* g = (GDALDatasetH*)GDALOpenEx(filename.c_str(),
        GDAL_OF_READONLY | GDAL_OF_RASTER,
        nullptr, nullptr, nullptr);
    if (g) {
        GDALClose(g);
        return std::make_shared<GDALFileImageProvider>(filename);
    }
}
#endif
iio2:
#ifdef USE_IIO
    return std::make_shared<IIOFileImageProvider>(filename);
#else
    return 0;
#endif
}

std::shared_ptr<ImageProvider> SingleImageImageCollection::getImageProvider(int index) const
{
    std::shared_ptr<ImageProvider> provider = selectProvider(filename);
    return provider;
}

std::shared_ptr<ImageProvider> EditedImageCollection::getImageProvider(int index) const
{
    std::vector<std::shared_ptr<ImageProvider>> providers;
    for (const auto& c : collections) {
        int iindex = std::min(index, c->getLength() - 1);
        providers.push_back(c->getImageProvider(iindex));
    }
    return std::make_shared<EditedImageProvider>(edittype, editprog, providers);
}

class VPPVideoImageProvider : public VideoImageProvider {
    FILE* file;
    int w, h, d;
    int curh;
    float* pixels;

public:
    VPPVideoImageProvider(const std::string& filename, int index, int w, int h, int d)
        : VideoImageProvider(filename, index)
        , file(fopen(filename.c_str(), "r"))
        , w(w)
        , h(h)
        , d(d)
        , curh(0)
    {
        fseek(file, 4 + 3 * sizeof(int) + w * h * d * sizeof(float) * index, SEEK_SET);
        pixels = (float*)malloc(w * h * d * sizeof(float));
    }

    ~VPPVideoImageProvider() override
    {
        if (pixels)
            free(pixels);
        fclose(file);
    }

    float getProgressPercentage() const override
    {
        return (float)curh / h;
    }

    void progress() override
    {
        if (curh < h) {
            if (!fread(pixels + curh * w * d, sizeof(float), w * d, file)) {
                onFinish(makeError("error vpp"));
            }
            curh++;
        } else {
            auto image = std::make_shared<Image>(pixels, w, h, d);
            onFinish(image);
            pixels = nullptr;
        }
    }
};

class VPPVideoImageCollection : public VideoImageCollection {
    size_t length;
    int error;
    int w, h, d;

public:
    VPPVideoImageCollection(const std::string& filename)
        : VideoImageCollection(filename)
        , length(0)
    {
        FILE* file = fopen(filename.c_str(), "r");
        std::array<char, 4> tag;
        if (fread(tag.data(), 1, 4, file) == 4
            && fread(&w, sizeof(int), 1, file)
            && fread(&h, sizeof(int), 1, file)
            && fread(&d, sizeof(int), 1, file)) {
            fseek(file, 0, SEEK_END);
            length = (ftell(file) - 4 - 3 * sizeof(int)) / (w * h * d * sizeof(float));
        }
        fclose(file);
    }

    ~VPPVideoImageCollection() override = default;

    int getLength() const override
    {
        return length;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) const override
    {
        return std::make_shared<VPPVideoImageProvider>(filename, index, w, h, d);
    }
};

#ifdef USE_IIO_NPY
extern "C" {
#include <npy.h>
}

class NumpyVideoImageProvider : public VideoImageProvider {
    int w, h, d;
    size_t length;
    struct npy_info ni;

public:
    NumpyVideoImageProvider(const std::string& filename, int index, int w, int h,
        int d, size_t length, struct npy_info ni)
        : VideoImageProvider(filename, index)
        , w(w)
        , h(h)
        , d(d)
        , length(length)
        , ni(ni)
    {
    }

    ~NumpyVideoImageProvider() override = default;

    float getProgressPercentage() const override
    {
        return 1.f;
    }

    void progress() override
    {
        FILE* file = fopen(filename.c_str(), "r");
        // compute frame position and read it
        size_t framesize = npy_type_size(ni.type) * w * h * d;
        long pos = ni.header_offset + frame * framesize;
        fseek(file, pos, SEEK_SET);
        void* data = malloc(framesize);
        if (fread(data, 1, framesize, file) != framesize) {
            onFinish(makeError("npy: couldn't read frame"));
        } else {
            // convert to float
            float* pixels = npy_convert_to_float(data, w * h * d, ni.type);
            auto image = std::make_shared<Image>(pixels, w, h, d);
            onFinish(image);
        }
        fclose(file);
    }
};

class NumpyVideoImageCollection : public VideoImageCollection {
    size_t length;
    int w, h, d;
    struct npy_info ni;

    void loadHeader()
    {
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
    NumpyVideoImageCollection(const std::string& filename)
        : VideoImageCollection(filename)
        , length(0)
    {
        loadHeader();
    }

    ~NumpyVideoImageCollection() override = default;

    int getLength() const override
    {
        return length;
    }

    std::shared_ptr<ImageProvider> getImageProvider(int index) const override
    {
        return std::make_shared<NumpyVideoImageProvider>(filename, index, w, h, d, length, ni);
    }
};
#endif

static std::shared_ptr<ImageCollection> selectCollection(const std::string& filename)
{
    fs::path path(filename);

    if (fs::is_regular_file(path)) {
        auto result = getFileTag(path);
        if (result) {
            auto tag = *result;
            if (tag[0] == 'V' && tag[1] == 'P' && tag[2] == 'P' && tag[3] == 0) {
                return std::make_shared<VPPVideoImageCollection>(filename);
            }
#ifdef USE_IIO_NPY
            if (tag[0] == 0x93 && tag[1] == 'N' && tag[2] == 'U' && tag[3] == 'M') {
                return std::make_shared<NumpyVideoImageCollection>(filename);
            }
#endif
        }
    }

    return std::make_shared<SingleImageImageCollection>(filename);
}

std::shared_ptr<ImageCollection> buildImageCollectionFromFilenames(const std::vector<std::string>& filenames)
{
    if (filenames.size() == 1) {
        return selectCollection(filenames[0]);
    }

    //!\  here we assume that a sequence composed of multiple files means that each file contains only one image (not true for video files)
    // the reason is just that it would be slow to check the tag of each file
    std::shared_ptr<MultipleImageCollection> collection = std::make_shared<MultipleImageCollection>();
    for (auto& f : filenames) {
#ifdef USE_IIO_NPY
        if (endswith(f, ".npy")) { // TODO: this is ugly, but faster than checking the tag
            collection->append(std::make_shared<NumpyVideoImageCollection>(f));
        } else {
#else
        {
#endif
            collection->append(std::make_shared<SingleImageImageCollection>(f));
        }
    }
    return collection;
}
