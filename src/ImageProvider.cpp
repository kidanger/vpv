#include <errno.h>

extern "C" {
#include "iio.h"
}

#include "Image.hpp"
#include "editors.hpp"
#include "ImageProvider.hpp"

void IIOFileImageProvider::progress()
{
    int w, h, d;
    float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
    if (!pixels) {
        fprintf(stderr, "cannot load image '%s'\n", filename.c_str());
        onFinish(makeError("cannot load image '" + filename + "'"));
        return;
    }

    if (d > 4) {
        printf("warning: '%s' has %d channels, extracting the first four\n", filename.c_str(), d);
        for (size_t y = 0; y < (size_t)h; y++) {
            for (size_t x = 0; x < (size_t)w; x++) {
                for (size_t l = 0; l < 4; l++) {
                    pixels[(y*h+x)*4+l] = pixels[(y*h+x)*d+l];
                }
            }
        }
        d = 4;
    }
    std::shared_ptr<Image> image = std::make_shared<Image>(pixels, w, h, d);
    onFinish(image);
}

#include <jpeglib.h>

static void onJPEGError(j_common_ptr cinfo)
{
    char buf[JMSG_LENGTH_MAX];
    JPEGFileImageProvider* provider = (JPEGFileImageProvider*) cinfo->client_data;
    (*cinfo->err->format_message)(cinfo, buf);
    provider->onJPEGError(buf);
}

JPEGFileImageProvider::~JPEGFileImageProvider()
{
    if (file) {
        fclose(file);
    }
    if (cinfo) {
        jpeg_abort((j_common_ptr) cinfo);
        delete cinfo;
    }
    if (jerr) {
        delete jerr;
    }
    if (pixels) {
        free(pixels);
    }
    if (scanline) {
        delete[] scanline;
    }
}

void JPEGFileImageProvider::onJPEGError(const std::string& error)
{
    onFinish(makeError(error));
    this->error = true;
}

float JPEGFileImageProvider::getProgressPercentage() const {
    if (cinfo) {
        return (float) cinfo->output_scanline / cinfo->output_height;
    } else {
        return 0.f;
    }
}

void JPEGFileImageProvider::progress()
{
    assert(!error);
    if (!cinfo) {
        file = fopen(filename.c_str(), "rb");
        if (!file) {
            onFinish(makeError(strerror(errno)));
            return;
        }
        cinfo = new struct jpeg_decompress_struct;
        cinfo->client_data = this;
        jerr = new jpeg_error_mgr;
        cinfo->err = jpeg_std_error(jerr);
        jerr->error_exit = ::onJPEGError;
        jpeg_create_decompress(cinfo);
        if (error) return;

        jpeg_stdio_src(cinfo, file);
        if (error) return;

        jpeg_read_header(cinfo, TRUE);
        if (error) return;

        jpeg_start_decompress(cinfo);
        if (error) return;

        pixels = (float*) malloc(sizeof(float)*cinfo->output_width*cinfo->output_height*cinfo->output_components);
        scanline = new unsigned char[cinfo->output_width*cinfo->output_components];
    } else if (cinfo->output_scanline < cinfo->output_height) {
        jpeg_read_scanlines(cinfo, &scanline, 1);
        if (error) return;
        size_t rowwidth = cinfo->output_width*cinfo->output_components;
        for (size_t j = 0; j < rowwidth; j++) {
            pixels[(size_t)(cinfo->output_scanline-1)*rowwidth + j] = scanline[j];
        }
    } else {
        jpeg_finish_decompress(cinfo);
        if (error) return;

        std::shared_ptr<Image> image = std::make_shared<Image>(pixels,
                               cinfo->output_width, cinfo->output_height, cinfo->output_components);
        onFinish(image);
        pixels = nullptr;
    }
}

#include <png.h>

PNGFileImageProvider::~PNGFileImageProvider()
{
    if (png_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    if (pngframe) {
        delete[] pngframe;
    }
    if (pixels) {
        free(pixels);
    }
    if (buffer) {
        delete[] buffer;
    }
    if (file) {
        fclose(file);
    }
}

float PNGFileImageProvider::getProgressPercentage() const
{
    if (height == 0)
        return 0.f;
    return (float) cur / height;
}

static void on_error(png_structp pp, const char* msg)
{
    void* userdata = png_get_progressive_ptr(pp);
    PNGFileImageProvider* provider = (PNGFileImageProvider*) userdata;
    provider->onPNGError(msg);
}

static void info_callback(png_structp png_ptr, png_infop info)
{
    void* userdata = png_get_progressive_ptr(png_ptr);
    PNGFileImageProvider* provider = (PNGFileImageProvider*) userdata;
    provider->info_callback();
}

void PNGFileImageProvider::info_callback()
{
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    channels = png_get_channels(png_ptr, info_ptr);
    depth = png_get_bit_depth(png_ptr, info_ptr);
    pixels = (float*) malloc(sizeof(float)*width*height*channels);
    pngframe = new unsigned char[width*height*channels*depth/8];

    png_start_read_image(png_ptr);
}

static void row_callback(png_structp png_ptr, png_bytep new_row,
                         png_uint_32 row_num, int pass)
{
    void* userdata = png_get_progressive_ptr(png_ptr);
    PNGFileImageProvider* provider = (PNGFileImageProvider*) userdata;
    provider->row_callback(new_row, row_num, pass);
}

void PNGFileImageProvider::row_callback(png_bytep new_row, png_uint_32 row_num, int pass)
{
    if (new_row) {
        png_progressive_combine_row(png_ptr, pngframe+row_num*width*channels*depth/8, new_row);
    }
    cur = row_num;
}

static void end_callback(png_structp png_ptr, png_infop info)
{
    void* userdata = png_get_progressive_ptr(png_ptr);
    PNGFileImageProvider* provider = (PNGFileImageProvider*) userdata;
    provider->end_callback();
}

void PNGFileImageProvider::end_callback()
{
}

int PNGFileImageProvider::initialize_png_reader()
{
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) this, on_error, NULL);
    if (!png_ptr)
        return 1;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL,
                                (png_infopp)NULL);
        return 1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr,
                                (png_infopp)NULL);
        return 2;
    }

    // see http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-3.10
    png_set_progressive_read_fn(png_ptr, this, ::info_callback, ::row_callback, ::end_callback);
    return 0;
}

void PNGFileImageProvider::progress()
{
    if (!file) {
        file = fopen(filename.c_str(), "rb");
        if (!file) {
            onFinish(makeError(strerror(errno)));
            return;
        }

        int ret = initialize_png_reader();
        if (ret == 1) {
            onFinish(makeError("cannot initialize png reader"));
        }
        if (ret != 0)
            return;

        length = 1<<12;
        buffer = new unsigned char[length];
        cur = 0;
    } else if (!feof(file)) {
        int read = fread(buffer, 1, length, file);

        if (ferror(file)) {
            onFinish(makeError(strerror(errno)));
            return;
        }

        if (!read) {
            onFinish(makeError(strerror(errno)));
            return;
        }

        // XXX: a jump point was set in initialize_png_reader
        // but if the caller changed, we are supposed to set one every call
        png_process_data(png_ptr, info_ptr, buffer, read);
    } else {
        switch (depth) {
            case 1:
            case 8:
                for (size_t i = 0; i < width*height*channels; i++) {
                    pixels[i] = *(pngframe + i);
                }
                break;
            case 16:
                for (size_t i = 0; i < width*height*channels; i++) {
                    png_byte *b = (pngframe + i * 2);
                    std::swap(b[0], b[1]);
                    uint16_t sample = *(uint16_t *)b;
                    pixels[i] = sample;
                }
                break;
            default:
                onFinish(makeError("unsuported bit depth " + std::to_string(depth)));
                return;
        }

        std::shared_ptr<Image> image = std::make_shared<Image>(pixels, width, height, channels);
        onFinish(image);
        pixels = nullptr;
    }
}

void PNGFileImageProvider::onPNGError(const std::string& error)
{
    onFinish(makeError(error));
    longjmp(png_jmpbuf(png_ptr), 1);
}


void EditedImageProvider::progress() {
    for (auto p : providers) {
        if (!p->isLoaded()) {
            p->progress();
            return;
        }
    }
    std::vector<std::shared_ptr<Image>> images;
    for (auto p : providers) {
        Result result = p->getResult();
        if (result.has_value()) {
            images.push_back(result.value());
        } else {
            onFinish(result);
            return;
        }
    }
    std::shared_ptr<Image> image = edit_images(edittype, editprog, images);
    onFinish(image);
}

