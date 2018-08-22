
extern "C" {
#include "iio.h"
}

#include "Image.hpp"
#include "editors.hpp"
#include "ImageProvider.hpp"

void IIOFileImageProvider::progress() {
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
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                for (int l = 0; l < 4; l++) {
                    pixels[(y*h+x)*4+l] = pixels[(y*h+x)*d+l];
                }
            }
        }
        d = 4;
    }
    Image* image = new Image(pixels, w, h, (Image::Format) d);
    onFinish(Result::makeResult(image));
}

void EditedImageProvider::progress() {
    for (auto p : providers) {
        if (!p->isLoaded()) {
            p->progress();
            return;
        }
    }
    std::vector<const Image*> images;
    for (auto p : providers) {
        Result result = p->getResult();
        if (result.isOK) {
            images.push_back(result.value);
        } else {
            onFinish(result);
            return;
        }
    }
    Image* image = edit_images(edittype, editprog, images);
    onFinish(Result::makeResult(image));
}

