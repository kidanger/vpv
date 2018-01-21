#pragma once

#include <vector>

struct Image;

enum EditType {
    PLAMBDA,
    GMIC,
    OCTAVE,
};

Image* edit_images(EditType edittype, const char* prog, std::vector<const Image*> images);

