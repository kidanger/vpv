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

static std::shared_ptr<Image> edit_images_plambda(const char* prog, const std::vector<std::shared_ptr<Image>>& images)
{
    size_t n = images.size();
    float* x[n];
    int w[n];
    int h[n];
    int d[n];
    for (size_t i = 0; i < n; i++) {
        std::shared_ptr<Image> img = images[i];
        x[i] = img->pixels;
        w[i] = img->w;
        h[i] = img->h;
        d[i] = img->format;
    }

    int dd;
    float* pixels = execute_plambda(n, x, w, h, d, (char*) prog, &dd);
    if (!pixels)
        return 0;

    std::shared_ptr<Image> img = std::make_shared<Image>(pixels, *w, *h, dd);
    return img;
}

static std::shared_ptr<Image> edit_images_gmic(const char* prog, const std::vector<std::shared_ptr<Image>>& images)
{
#ifdef USE_GMIC
    gmic_list<char> images_names;
    gmic_list<float> gimages;
    gimages.assign(images.size());
    for (size_t i = 0; i < images.size(); i++) {
    std::shared_ptr<Image> img = images[i];
        gmic_image<float>& gimg = gimages[i];
        gimg.assign(img->w, img->h, 1, img->format);
        const float* xptr = img->pixels;
        for (size_t y = 0; y < img->h; y++) {
            for (size_t x = 0; x < img->w; x++) {
                for (size_t z = 0; z < img->format; z++) {
                    gimg(x, y, 0, z) = *(xptr++);
                }
            }
        }
    }

    try {
        gmic(prog, gimages, images_names);
    } catch (gmic_exception &e) {
        std::fprintf(stderr,"\n- Error encountered when calling G'MIC : '%s'\n", e.what());
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

    std::shared_ptr<Image> img = std::make_shared<Image(data, w, h, d);
    return img;
#else
    fprintf(stderr, "not compiled with GMIC support\n");
    return 0;
#endif
}

static std::shared_ptr<Image> edit_images_octave(const char* prog, const std::vector<std::shared_ptr<Image>>& images)
{
#ifdef USE_OCTAVE
    static octave::interpreter* app;

    if (!app) {
        app = new octave::interpreter();
        app->initialize_history(false);
        app->initialize();
        app->interactive(false);
    }

    try {
        octave_value_list in;

        // create the function
        octave_value_list in2;
        in2(0) = octave_value(std::string(prog));
        octave_value_list fs = Fstr2func(in2);
        octave_function* f = fs(0).function_value();

        // create the matrices
        for (size_t i = 0; i < images.size(); i++) {
            std::shared_ptr<Image> img = images[i];
            dim_vector size((int)img->h, (int)img->w, (int)img->format);
            NDArray m(size);

            float* xptr = img->pixels;
            for (size_t y = 0; y < img->h; y++) {
                for (size_t x = 0; x < img->w; x++) {
                    for (size_t z = 0; z < img->format; z++) {
                        m(y, x, z) = *(xptr++);
                    }
                }
            }

            in(i) = octave_value(m);
        }

        // eval
        octave_value_list out = octave::feval(f, in, 1);

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
            std::cerr << "no image returned from octave\n";
        }

    } catch (const octave::exit_exception& ex) {
        exit (ex.exit_status());
        return 0;

    } catch (const octave::execution_exception&) {
        std::cerr << "error evaluating Octave code!" << std::endl;
        return 0;
    }
#else
    fprintf(stderr, "not compiled with octave support\n");
#endif
    return 0;
}

std::shared_ptr<Image> edit_images(EditType edittype, const std::string& _prog,
                                   const std::vector<std::shared_ptr<Image>>& images)
{
    char* prog = (char*) _prog.c_str();
    switch (edittype) {
        case PLAMBDA:
            return edit_images_plambda(prog, images);
        case GMIC:
            return edit_images_gmic(prog, images);
        case OCTAVE:
            return edit_images_octave(prog, images);
    }
    return 0;
}

#include "ImageCollection.hpp"
#include "Sequence.hpp"
#include "globals.hpp"

ImageCollection* create_edited_collection(EditType edittype, const std::string& _prog)
{
    char* prog = (char*) _prog.c_str();
    std::vector<Sequence*> sequences;
    while (*prog && *prog != ' ') {
        char* old = prog;
        int a = strtol(prog, &prog, 10) - 1;
        if (prog == old) break;
        if (a < 0 || a >= gSequences.size()) return 0;
        sequences.push_back(gSequences[a]);
        if (*prog == ' ') break;
        if (*prog) prog++;
    }
    while (*prog == ' ') prog++;

    std::vector<ImageCollection*> collections;
    for (auto s : sequences) {
        collections.push_back(s->uneditedCollection);
    }
    return new EditedImageCollection(edittype, std::string(prog), collections);
}

