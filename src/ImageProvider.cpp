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
    printf("loading '%s'\n", filename.c_str());
    float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
    if (!pixels) {
        fprintf(stderr, "cannot load image '%s'\n", filename.c_str());
        onFinish(Result::makeError(new std::runtime_error("cannot load image '" + filename + "'")));
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
    onFinish(Result::makeResult(image));
}

extern "C" {
#include <jpeglib.h>
}

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
    onFinish(Result::makeError(new std::runtime_error(error)));
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
            onFinish(Result::makeError(new std::runtime_error(strerror(errno))));
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
        onFinish(Result::makeResult(image));
        pixels = nullptr;
    }
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
        if (result.isOK) {
            images.push_back(result.value);
        } else {
            onFinish(result);
            return;
        }
    }
    std::shared_ptr<Image> image = edit_images(edittype, editprog, images);
    onFinish(Result::makeResult(image));
}

