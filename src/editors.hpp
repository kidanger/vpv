#pragma once

#include <vector>

struct Image;

enum EditType {
    PLAMBDA,
    GMIC,
    OCTAVE,
};

Image* run_edit_program(char* prog, EditType edittype);

Image* edit_images(EditType edittype, const char* prog, std::vector<const Image*> images);

