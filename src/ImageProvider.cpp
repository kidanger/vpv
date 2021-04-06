#include <cerrno>
#include <memory>

#ifdef USE_IIO
extern "C" {
#include <iio.h>
}
#endif

#include "Image.hpp"
#include "editors.hpp"
#include "ImageProvider.hpp"

#ifdef USE_IIO
static std::shared_ptr<Image> load_from_iio(const std::string& filename)
{
    int w, h, d;
    float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
    if (!pixels) {
       return nullptr;
    }

    return std::make_shared<Image>(pixels, w, h, d);
}

void IIOFileImageProvider::progress()
{
    std::shared_ptr<Image> image = load_from_iio(filename);
    if (!image) {
        onFinish(makeError("cannot load image '" + filename + "'"));
    } else {
        onFinish(image);
    }
}
#endif

#ifdef USE_GDAL
#include <gdal.h>
#include <gdal_priv.h>
void GDALFileImageProvider::progress()
{
    GDALDataset* g = (GDALDataset*) GDALOpen(filename.c_str(), GA_ReadOnly);
    if (!g) {
        onFinish(makeError("gdal: cannot load image '" + filename + "'"));
        return;
    }

    int w = g->GetRasterXSize();
    int h = g->GetRasterYSize();
    int d = g->GetRasterCount();
    int tf = 1;
    GDALDataType asktype = GDT_Float32;
    if (d == 1) {
        GDALRasterBand* band = g->GetRasterBand(1);
        GDALDataType type = band->GetRasterDataType();
        if (GDALDataTypeIsComplex(type)) {
            asktype = GDT_CFloat32;
            tf = 2;
        }
    }
    float* pixels = (float*) malloc(sizeof(float) * w * h * d * tf);
    GDALRasterIOExtraArg args;
    INIT_RASTERIO_EXTRA_ARG(args);
    args.pfnProgress = [](double d, const char*, void* data){
        ((GDALFileImageProvider*) data)->df = d;
        return 1;
    };
    args.pProgressData = this;
    CPLErr err = g->RasterIO(GF_Read, 0, 0, w, h, pixels, w, h, asktype, d,
                             nullptr, sizeof(float)*d*tf, sizeof(float)*w*d*tf, sizeof(float)*tf,
                             &args);
    d *= tf;
    GDALClose(g);

    if (err != CE_None) {
        onFinish(makeError("gdal: cannot load image '" + filename +
                           "' err:" + std::to_string(err)));
    } else {
        std::shared_ptr<Image> image = std::make_shared<Image>(pixels, w, h, d);
        onFinish(image);
    }
}
#endif


extern "C" {
    #include <jpeglib.h>
}

class JPEGFileImageProvider::impl {
public:
    impl(JPEGFileImageProvider *provider)
        : cinfo(), file(nullptr),
        pixels(nullptr), scanline(nullptr), error(false), jerr(), provider(provider)
    {
        cinfo.client_data = this;
        jerr.error_exit = &impl::onJPEGError;
        cinfo.err = jpeg_std_error(&jerr);
    }

    ~impl()
    {
        if (file) {
            fclose(file);
        }
        if (pixels) {
            free(pixels);
        }
        jpeg_abort((j_common_ptr) &cinfo);
    }

    void onJPEGError(const std::string& error)
    {
        provider->onFinish(makeError(error));
        this->error = true;
    }

    float getProgressPercentage() const {
        if (pixels) {
            return (float)cinfo.output_scanline / cinfo.output_height;
        }
        else {
            return 0.f;
        }
    }

    void progress()
    {
        assert(!error);
        if (!pixels) {
            file = fopen(provider->filename.c_str(), "rb");
            if (!file) {
                provider->onFinish(makeError(strerror(errno)));
                return;
            }
            jpeg_create_decompress(&cinfo);
            if (error) return;

            jpeg_stdio_src(&cinfo, file);
            if (error) return;

            jpeg_read_header(&cinfo, TRUE);
            if (error) return;

            jpeg_start_decompress(&cinfo);
            if (error) return;

            pixels = (float*) malloc(sizeof(float)*cinfo.output_width*cinfo.output_height*cinfo.output_components);
            scanline = std::unique_ptr<unsigned char[]>(new unsigned char[cinfo.output_width * cinfo.output_components]);
        } else if (cinfo.output_scanline < cinfo.output_height) {
            JSAMPROW sample = scanline.get();
            jpeg_read_scanlines(&cinfo, &sample, 1);
            if (error) return;
            size_t rowwidth = cinfo.output_width * cinfo.output_components;
            for (size_t j = 0; j < rowwidth; j++) {
                pixels[(size_t)(cinfo.output_scanline - 1) * rowwidth + j] = scanline[j];
            }
        } else {
            jpeg_finish_decompress(&cinfo);
            if (error) return;

            std::shared_ptr<Image> image = std::make_shared<Image>(pixels,
                cinfo.output_width, cinfo.output_height, cinfo.output_components);
            provider->onFinish(image);
            pixels = nullptr;
        }
    }

private:
    static void onJPEGError(j_common_ptr cinfo)
    {
        char buf[JMSG_LENGTH_MAX];
        impl* provider = (impl*) cinfo->client_data;
        (*cinfo->err->format_message)(cinfo, buf);
        provider->onJPEGError(buf);
    }

    struct jpeg_decompress_struct cinfo;
    FILE* file;
    float *pixels;
    std::unique_ptr<unsigned char[]> scanline;
    bool error;
    struct jpeg_error_mgr jerr;
    JPEGFileImageProvider* provider;
};

JPEGFileImageProvider::JPEGFileImageProvider(const std::string& filename)
    : FileImageProvider(filename), pimpl(new impl(this))
{
}

JPEGFileImageProvider::~JPEGFileImageProvider()
{

}

float JPEGFileImageProvider::getProgressPercentage() const {
    return pimpl->getProgressPercentage();
}

void JPEGFileImageProvider::progress()
{
    return pimpl->progress();
}

#include <png.h>

struct PNGPrivate {
    PNGFileImageProvider* provider;

    FILE* file;
    png_structp png_ptr;
    png_infop info_ptr;
    uint32_t width, height;
    int channels;
    int depth;
    uint32_t cur;
    float* pixels;
    std::unique_ptr<png_byte[]> pngframe;

    uint32_t length;
    std::unique_ptr<png_byte[]> buffer;

    PNGPrivate(PNGFileImageProvider* provider)
        : provider(provider), file(nullptr), png_ptr(nullptr), info_ptr(nullptr),
          height(0), pixels(nullptr), pngframe(nullptr),  buffer(nullptr)
    {}

    ~PNGPrivate() {
        if (png_ptr) {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        }
        if (pixels) {
            free(pixels);
        }
        if (file) {
            fclose(file);
        }
    }

    void info_callback()
    {
        width = png_get_image_width(png_ptr, info_ptr);
        height = png_get_image_height(png_ptr, info_ptr);
        channels = png_get_channels(png_ptr, info_ptr);
        depth = png_get_bit_depth(png_ptr, info_ptr);
        pixels = (float*) malloc(sizeof(float)*width*height*channels);
        pngframe = std::unique_ptr<png_byte[]>(new png_byte[width * height * channels * depth / 8]);

        if (png_get_interlace_type(png_ptr, info_ptr) != PNG_INTERLACE_NONE) {
            png_set_interlace_handling(png_ptr);
        }

        png_start_read_image(png_ptr);
    }

    void row_callback(png_bytep new_row, png_uint_32 row_num, int pass)
    {
        if (new_row) {
            // TODO: is this valid for 1bit/pixel ?
            png_progressive_combine_row(png_ptr, pngframe.get() + row_num * width * channels * depth/8, new_row);
        }
        cur = row_num;
    }

    void end_callback()
    {
    }

    std::shared_ptr<Image> getImage()
    {
        switch (depth) {
            case 1:
                for (size_t i = 0; i < width*height*channels/8; i++) {
                    for (int b = 7; b >= 0; b--)
                        pixels[i*8 + 7 - b] = !!(pngframe[i] & (1<<b));
                }
                break;
            case 8:
                for (size_t i = 0; i < width*height*channels; i++) {
                    pixels[i] = pngframe[i];
                }
                break;
            case 16:
                for (size_t i = 0; i < width*height*channels; i++) {
                    png_byte *b = pngframe.get() + i * 2;
                    std::swap(b[0], b[1]);
                    uint16_t sample = *(uint16_t *)b;
                    pixels[i] = sample;
                }
                break;
            default:
                return nullptr;
        }

        auto img = std::make_shared<Image>(pixels, width, height, channels);
        pixels = nullptr;
        return img;
    }
};

PNGFileImageProvider::~PNGFileImageProvider()
{
    if (p) delete p;
}

float PNGFileImageProvider::getProgressPercentage() const
{
    if (!p || p->height == 0)
        return 0.f;
    return (float) p->cur / p->height;
}

static void on_error(png_structp pp, const char* msg)
{
    void* userdata = png_get_progressive_ptr(pp);
    PNGPrivate* p = (PNGPrivate*) userdata;
    p->provider->onPNGError(msg);
}

static void info_callback(png_structp png_ptr, png_infop info)
{
    void* userdata = png_get_progressive_ptr(png_ptr);
    PNGPrivate* p = (PNGPrivate*) userdata;
    p->info_callback();
}

static void row_callback(png_structp png_ptr, png_bytep new_row,
                         png_uint_32 row_num, int pass)
{
    void* userdata = png_get_progressive_ptr(png_ptr);
    PNGPrivate* p = (PNGPrivate*) userdata;
    p->row_callback(new_row, row_num, pass);
}

static void end_callback(png_structp png_ptr, png_infop info)
{
    void* userdata = png_get_progressive_ptr(png_ptr);
    PNGPrivate* p = (PNGPrivate*) userdata;
    p->end_callback();
}


int PNGFileImageProvider::initialize_png_reader()
{
    p->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) p, on_error, nullptr);
    if (!p->png_ptr)
        return 1;

    p->info_ptr = png_create_info_struct(p->png_ptr);
    if (!p->info_ptr) {
        png_destroy_read_struct(&p->png_ptr, (png_infopp)nullptr,
                                (png_infopp)nullptr);
        return 1;
    }

    if (setjmp(png_jmpbuf(p->png_ptr))) {
        return 2;
    }

    // see http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-3.10
    png_set_progressive_read_fn(p->png_ptr, p, ::info_callback, ::row_callback, ::end_callback);
    return 0;
}

void PNGFileImageProvider::progress()
{
    if (!p) {
        p = new PNGPrivate(this);
        p->file = fopen(filename.c_str(), "rb");
        if (!p->file) {
            onFinish(makeError(strerror(errno)));
            return;
        }

        int ret = initialize_png_reader();
        if (ret != 0) {
            onFinish(makeError("cannot initialize png reader"));
            return;
        }

        p->length = 1<<12;
        p->buffer = std::unique_ptr<png_byte[]>(new png_byte[p->length]);
        p->cur = 0;
    } else if (!feof(p->file)) {
        int read = fread(p->buffer.get(), 1, p->length, p->file);

        if (ferror(p->file)) {
            onFinish(makeError(strerror(errno)));
            return;
        }

        if (setjmp(png_jmpbuf(p->png_ptr))) {
            return;
        }

        png_process_data(p->png_ptr, p->info_ptr, p->buffer.get(), read);
    } else {
        std::shared_ptr<Image> image = p->getImage();
        if (!image) {
            onFinish(makeError("error png invalid depth(?)"));
        } else {
            onFinish(image);
        }
    }
}

void PNGFileImageProvider::onPNGError(const std::string& error)
{
    onFinish(makeError(error));
    longjmp(png_jmpbuf(p->png_ptr), 1);
}


#include <tiffio.h>

struct TIFFPrivate {
    TIFFFileImageProvider* provider;
    TIFF* tif;
    uint32_t w, h;
    uint16_t spp, bps, fmt;
    float* data;
    std::unique_ptr<uint8_t> buf;
    bool broken;
    uint32_t curh;
    int sls;

    TIFFPrivate(TIFFFileImageProvider* provider)
        : provider(provider), tif(nullptr), h(0), data(nullptr), buf(nullptr), curh(0)
    {
    }

    ~TIFFPrivate()
    {
        if (tif) {
            TIFFClose(tif);
        }
        if (data)
            free(data);
    }
};

TIFFFileImageProvider::~TIFFFileImageProvider()
{
    if (p) {
        delete p;
    }
}

float TIFFFileImageProvider::getProgressPercentage() const
{
    if (p && p->h)
        return (float) p->curh / p->h;
    return 0.f;
}

void TIFFFileImageProvider::progress()
{
    if (!p) {
        p = new TIFFPrivate(this);
        p->tif = TIFFOpen(filename.c_str(), "rm");
        if (!p->tif) return onFinish(makeError("cannot read tiff " + filename));

        int r = 0;
        r += TIFFGetField(p->tif, TIFFTAG_IMAGEWIDTH, &p->w);
        r += TIFFGetField(p->tif, TIFFTAG_IMAGELENGTH, &p->h);

        if (r != 2) return onFinish(makeError("can not read tiff of unknown size"));

        r = TIFFGetField(p->tif, TIFFTAG_SAMPLESPERPIXEL, &p->spp);
        if (!r)
            p->spp=1;

        r = TIFFGetField(p->tif, TIFFTAG_BITSPERSAMPLE, &p->bps);
        if (!r)
            p->bps=1;

        r = TIFFGetField(p->tif, TIFFTAG_SAMPLEFORMAT, &p->fmt);
        if (!r)
            p->fmt = SAMPLEFORMAT_UINT;

        if (p->fmt == SAMPLEFORMAT_COMPLEXINT || p->fmt == SAMPLEFORMAT_COMPLEXIEEEFP) {
            p->spp *= 2;
            p->bps /= 2;
        }
        if (p->fmt == SAMPLEFORMAT_COMPLEXINT)
            p->fmt = SAMPLEFORMAT_INT;
        if (p->fmt == SAMPLEFORMAT_COMPLEXIEEEFP)
            p->fmt = SAMPLEFORMAT_IEEEFP;

        uint16_t planarity;
        r = TIFFGetField(p->tif, TIFFTAG_PLANARCONFIG, &planarity);
        if (r != 1) planarity = PLANARCONFIG_CONTIG;
        p->broken = planarity == PLANARCONFIG_SEPARATE;

        uint32_t scanline_size = (p->w * p->spp * p->bps)/8;
        int rbps = (p->bps/8) ? (p->bps/8) : 1;
        p->sls = TIFFScanlineSize(p->tif);
        if ((int)scanline_size != p->sls)
            fprintf(stderr, "scanline_size,sls = %d,%d\n", (int)scanline_size, p->sls);
        //assert((int)scanline_size == sls);
        if (!p->broken)
            assert((int)scanline_size == p->sls);
        assert((int)scanline_size >= p->sls);
        p->data = (float*) malloc(p->w * p->h * p->spp * rbps);
        p->buf = std::unique_ptr<uint8_t>(new uint8_t[scanline_size]);
        p->curh = 0;

        if (TIFFIsTiled(p->tif) || p->fmt != SAMPLEFORMAT_IEEEFP || p->broken || rbps != sizeof(float)) {
#ifdef USE_IIO
            std::shared_ptr<Image> image = load_from_iio(filename);
            if (!image) {
                onFinish(makeError("iio: cannot load image '" + filename + "'"));
            } else {
                onFinish(image);
            }
#else
            onFinish(makeError("cannot load image '" + filename + "'"));
#endif
        }
    } else if (p->curh < p->h) {
        int r = TIFFReadScanline(p->tif, p->buf.get(), p->curh);
        if (r < 0) onFinish(makeError("error reading tiff row " + std::to_string(p->curh)));
        memcpy(p->data + p->curh * p->sls/sizeof(float), p->buf.get(), p->sls);
        p->curh++;
    } else {
        std::shared_ptr<Image> image = std::make_shared<Image>(p->data, p->w, p->h, p->spp);
        onFinish(image);
        p->data = nullptr;
    }
}

#ifdef USE_LIBRAW
#include <libraw/libraw.h>
#endif

bool RAWFileImageProvider::canOpen(const std::string& filename)
{
#ifdef USE_LIBRAW
    LibRaw processor;
    return processor.open_file(filename.c_str()) == LIBRAW_SUCCESS;
#else
    return false;
#endif
}

RAWFileImageProvider::~RAWFileImageProvider()
{
}

float RAWFileImageProvider::getProgressPercentage() const
{
    return 0.f;
}

// adapted from pvflip/piio
void RAWFileImageProvider::progress()
{
#ifdef USE_LIBRAW
    LibRaw* processor = new LibRaw;
    int ret;
    if ((ret = processor->open_file(filename.c_str())) != LIBRAW_SUCCESS) {
        onFinish(makeError("libraw: cannot open " + filename + " " + libraw_strerror(ret)));
        goto end;
    }

    if ((ret = processor->unpack() ) != LIBRAW_SUCCESS) {
        onFinish(makeError("libraw: cannot unpack " + filename + " " + libraw_strerror(ret)));
        goto end;
    }

    if (!processor->imgdata.idata.filters && processor->imgdata.idata.colors != 1) {
        onFinish(makeError("libraw: only bayer-pattern RAW files supported"));
        goto end;
    }

    {
        int w = processor->imgdata.sizes.raw_width;
        int h = processor->imgdata.sizes.raw_height;
        int d = 1;
        float* data = (float*) malloc(sizeof(float)*w*h*d);

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                data[y*w+x] = processor->imgdata.rawdata.raw_image[y*w+x];
            }
        }

        std::shared_ptr<Image> image = std::make_shared<Image>(data, w, h, d);
        onFinish(image);
    }
end:
    delete processor;
#endif
}

void EditedImageProvider::progress() {
    for (const auto& p : providers) {
        if (!p->isLoaded()) {
            p->progress();
            return;
        }
    }
    std::vector<std::shared_ptr<Image>> images;
    for (const auto& p : providers) {
        Result result = p->getResult();
        if (result.has_value()) {
            std::shared_ptr<Image> image = result.value();
            image->usedBy.insert(key);
            images.push_back(image);
        } else {
            onFinish(result);
            return;
        }
    }
    std::string error;
    std::shared_ptr<Image> image = edit_images(edittype, editprog, images, error);
    if (image) {
        onFinish(image);
    } else {
        onFinish(makeError("cannot edit: " + error));
    }
}

