#include <iostream>

#include "Image.hpp"

#include "plambda.h"
#ifdef USE_GMIC
#include "gmic.h"
#endif

#ifdef USE_OCTAVE
#include <octave/oct.h>
#include <octave/octave.h>
#include <octave/parse.h>
#include <octave/interpreter.h>
#include <octave/builtin-defun-decls.h>
#endif

#include "editors.hpp"

static std::shared_ptr<Image> edit_images_plambda(const char* prog,
                              const std::vector<std::shared_ptr<Image>>& images,
                              std::string& error)
{
    size_t n = images.size();
    std::vector<float*> x(n);
    std::vector<int> w(n);
    std::vector<int> h(n);
    std::vector<int> d(n);
    for (size_t i = 0; i < n; i++) {
        std::shared_ptr<Image> img = images[i];
        x[i] = img->pixels;
        w[i] = img->w;
        h[i] = img->h;
        d[i] = img->c;
    }

    int dd;
    char* err;
    float* pixels = execute_plambda(n, &x[0], &w[0], &h[0], &d[0],
                                    (char*) prog, &dd, &err);
    if (!pixels) {
        error = std::string(err);
        return 0;
    }

    std::shared_ptr<Image> img = std::make_shared<Image>(pixels, w[0], h[0], dd);
    return img;
}

static std::shared_ptr<Image> edit_images_gmic(const char* prog,
                               const std::vector<std::shared_ptr<Image>>& images,
                               std::string& error)
{
#ifdef USE_GMIC
    gmic_list<char> images_names;
    gmic_list<float> gimages;
    gimages.assign(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        std::shared_ptr<Image> img = images[i];
        gmic_image<float>& gimg = gimages[i];
        gimg.assign(img->w, img->h, 1, img->c);
        const float* xptr = img->pixels;
        for (size_t y = 0; y < img->h; y++) {
            for (size_t x = 0; x < img->w; x++) {
                for (size_t z = 0; z < img->c; z++) {
                    gimg(x, y, 0, z) = *(xptr++);
                }
            }
        }
    }

    try {
        gmic(prog, gimages, images_names);
    } catch (gmic_exception &e) {
        error = e.what();
        std::cerr << "gmic: " << error << std::endl;
        return 0;
    }

    gmic_image<float>& image = gimages[0];
    size_t size = image._width * image._height * image._spectrum;
    float* data = (float*) malloc(sizeof(float) * size);
    float* ptrdata = data;
    for (size_t y = 0; y < image._height; y++) {
        for (size_t x = 0; x < image._width; x++) {
            for (size_t z = 0; z < image._spectrum; z++) {
                *(ptrdata++) = image(x, y, 0, z);
            }
        }
    }

    std::shared_ptr<Image> img = std::make_shared<Image>(data, image._width,
                                                        image._height, image._spectrum);
    return img;
#else
    error = "not compiled with GMIC support";
    std::cerr << error << std::endl;
    return 0;
#endif
}

static std::shared_ptr<Image> edit_images_octave(const char* prog,
                             const std::vector<std::shared_ptr<Image>>& images,
                             std::string& error)
{
#ifdef USE_OCTAVE
#if OCTAVE_MAJOR_VERSION == 4 && OCTAVE_MINOR_VERSION == 2 && OCTAVE_PATCH_VERSION == 2
    static octave::embedded_application* app;

    if (!app) {
        string_vector octave_argv(2);
        octave_argv(0) = "embedded";
        octave_argv(1) = "-q";
        app = new octave::embedded_application(2, octave_argv.c_str_vec());

        if (!app->execute()) {
            std::cerr << "creating embedded Octave interpreter failed!" << std::endl;
            return 0;
        }
    }
#else
    static octave::interpreter* app;

    if (!app) {
        app = new octave::interpreter();
        app->initialize_history(false);
        app->initialize();
        app->interactive(false);
        octave::source_file("~/.octaverc", "", false /*verbose*/, false /* required*/);
    }
#endif


    try {
        octave_value_list in;

        // create the function
        octave_value_list in2;
        in2(0) = octave_value(std::string(prog));
#if OCTAVE_MAJOR_VERSION == 4 && OCTAVE_MINOR_VERSION <= 4
        octave_value_list fs = Fstr2func(in2);
#else
        octave_value_list fs = Fstr2func(*app, in2);
#endif
        octave_function* f = fs(0).function_value();

        // create the matrices
        for (size_t i = 0; i < images.size(); i++) {
            std::shared_ptr<Image> img = images[i];
            dim_vector size((int)img->h, (int)img->w, (int)img->c);
            NDArray m(size);

            float* xptr = img->pixels;
            for (size_t y = 0; y < img->h; y++) {
                for (size_t x = 0; x < img->w; x++) {
                    for (size_t z = 0; z < img->c; z++) {
                        m(y, x, z) = *(xptr++);
                    }
                }
            }

            in(i) = octave_value(m);
        }

        // eval
#if OCTAVE_MAJOR_VERSION == 4 && OCTAVE_MINOR_VERSION == 2
        octave_value_list out = feval(f, in, 1);
#else
        octave_value_list out = octave::feval(f, in, 1);
#endif

        if (out.length() > 0) {
            NDArray m = out(0).array_value();
            size_t w = m.cols();
            size_t h = m.rows();
            size_t d = m.ndims() == 3 ? m.pages() : 1;
            size_t size = w * h * d;
            float* data = (float*) malloc(sizeof(float) * size);
            float* ptrdata = data;
            for (size_t y = 0; y < h; y++) {
                for (size_t x = 0; x < w; x++) {
                    for (size_t z = 0; z < d; z++) {
                        *(ptrdata++) = m(y, x, z);
                    }
                }
            }
            std::shared_ptr<Image> img = std::make_shared<Image>(data, w, h, d);
            return img;
        } else {
            error = "no image returned from octave";
            std::cerr << error << std::endl;
        }

    } catch (const octave::exit_exception& ex) {
        exit (ex.exit_status());
        return 0;

    } catch (const octave::execution_exception& ex) {
        std::cerr << "octave execution_exception" << std::endl;
        return 0;
    }
#else
    fprintf(stderr, "not compiled with octave support\n");
#endif
    return 0;
}

std::shared_ptr<Image> edit_images(EditType edittype, const std::string& _prog,
                                   const std::vector<std::shared_ptr<Image>>& images,
                                   std::string& error)
{
    char* prog = (char*) _prog.c_str();
    std::shared_ptr<Image> image;
    switch (edittype) {
        case PLAMBDA:
            image = edit_images_plambda(prog, images, error);
            break;
        case GMIC:
            image = edit_images_gmic(prog, images, error);
            break;
        case OCTAVE:
            image = edit_images_octave(prog, images, error);
            break;
    }
    return image;
}

#include "ImageCollection.hpp"
#include "Sequence.hpp"
#include "globals.hpp"

std::shared_ptr<ImageCollection> create_edited_collection(EditType edittype, const std::string& _prog)
{
    char* prog = (char*) _prog.c_str();
    std::vector<std::shared_ptr<ImageCollection>> collections;
    while (*prog && *prog != ' ') {
        char* old = prog;
        int a = strtol(prog, &prog, 10) - 1;
        if (prog == old) break;
        if (a >= 0 && a < gSequences.size()) {
            Sequence* s = gSequences[a];
            std::shared_ptr<ImageCollection> c(s->uneditedCollection);
            if (*prog == '@') {
                prog++;
                int relative = *prog == '-' || *prog == '+';
                char* old = prog;
                int a = strtol(prog, &prog, 10);
                int value = a;
                if (prog == old) break;

                if (relative) {
                    c = std::make_shared<OffsetedImageCollection>(c, value);
                } else {
                    c = std::make_shared<FixedImageCollection>(c, value);
                }
            }
            collections.push_back(c);
        }
        if (*prog == ' ') break;
        if (*prog) prog++;
    }
    while (*prog == ' ') prog++;

    if (collections.empty()) {
        return nullptr;
    }

    return std::make_shared<EditedImageCollection>(edittype, std::string(prog), collections);
}

