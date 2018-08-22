#pragma once

#include <vector>
#include <string>

struct Image;

enum EditType {
    PLAMBDA,
    GMIC,
    OCTAVE,
};

Image* run_edit_program(char* prog, EditType edittype);

Image* edit_images(EditType edittype, const std::string& prog, std::vector<const Image*> images);

class ImageCollection* create_edited_collection(EditType edittype, const std::string& prog);

