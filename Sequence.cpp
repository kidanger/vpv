#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <glob.h>

#include <SFML/OpenGL.hpp>

extern "C" {
#include "iio.h"
}

#include "Sequence.hpp"
#include "Player.hpp"
#include "View.hpp"
#include "alphanum.hpp"

Sequence::Sequence()
{
    static int id = 0;
    id++;
    ID = "Sequence " + std::to_string(id);

    view = nullptr;
    player = nullptr;
    valid = false;
    visible = false;

    loadedFrame = -1;
    loadedRect = ImRect();

    glob.reserve(1024);
    glob_.reserve(1024);
    glob = "";
    glob_ = "";
}

void Sequence::loadFilenames() {
    glob_t res;
    ::glob(glob.c_str(), GLOB_TILDE | GLOB_NOSORT, NULL, &res);
    filenames.resize(res.gl_pathc);
    for(unsigned int j = 0; j < res.gl_pathc; j++) {
        filenames[j] = res.gl_pathv[j];
    }
    globfree(&res);
    std::sort(filenames.begin(), filenames.end(), doj::alphanum_less<std::string>());

    valid = filenames.size() > 0;
    strcpy(&glob_[0], &glob[0]);

    loadedFrame = -1;
}

void Sequence::loadFrame(int frame)
{
    if (pixelCache.count(frame)) {
        return;
    }

    const std::string& filename = filenames[frame - 1];

    sf::Image img;
    if (!img.loadFromFile(filename)) {
        int w, h, d;
        float* pixels = iio_read_image_float_vec(filename.c_str(), &w, &h, &d);
        float min = 0;
        float max = FLT_MIN;
        for (int i = 0; i < w*h*d; i++) {
            min = fminf(min, pixels[i]);
            max = fmaxf(max, pixels[i]);
        }
        float a = 1.f;
        float b = 0.f;
        if (fabsf(min - 0.f) < 0.01f && fabsf(max - 1.f) < 0.01f) {
            a = 255.f;
        } else {
            a = 255.f / (max - min);
            b = - min;
        }
        uint8_t* rgba = new unsigned char[w * h * 4];
        for (int i = 0; i < w*h; i++) {
            rgba[i * 4 + 0] = b + a*pixels[i * d + 0];
            if (d > 1)
                rgba[i * 4 + 1] = b + a*pixels[i * d + 1];
            if (d > 2)
                rgba[i * 4 + 2] = b + a*pixels[i * d + 2];
            for (int dd = d; dd < 3; dd++) {
                rgba[i * 4 + dd] = rgba[i * 4];
            }
            rgba[i * 4 + 3] = 255;
        }
        free(pixels);

        img.create(w, h, rgba);
        free(rgba);
    }

    pixelCache[frame] = img;
}

void Sequence::loadTextureIfNeeded()
{
    if (valid && visible && player) {
        int frame = player->frame;

        if (!pixelCache.count(frame)) {
            loadFrame(frame);
            if (!pixelCache.count(frame)) {
                return;
            }
        }

        if (loadedFrame != player->frame) {
            loadedRect = ImRect();
        }

        const sf::Image& img = pixelCache[frame];
        int w = img.getSize().x;
        int h = img.getSize().y;
        bool reupload = false;

        ImVec2 u, v;
        view->compute(img.getSize(), u, v);

        ImRect area(u.x*w, u.y*h, v.x*w+1, v.y*h+1);
        area.Floor();
        area.Clip(ImRect(0, 0, w, h));

        if (!loadedRect.ContainsInclusive(area)) {
            reupload = true;
        }

        if (reupload) {
            area.Expand(32);  // to avoid multiple uploads during zoom-out
            area.Clip(ImRect(0, 0, w, h));

            if (texture.getSize().x != w || texture.getSize().y != h) {
                texture.create(w, h);
            }

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            const uint8_t* data = img.getPixelsPtr() + (w * (int)area.Min.y + (int)area.Min.x)*4;
            texture.update(data, area.GetWidth(), area.GetHeight(), area.Min.x, area.Min.y);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            loadedFrame = frame;
            loadedRect.Add(area);
        }
    }
}

