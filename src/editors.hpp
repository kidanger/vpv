#pragma once

#include <vector>
#include <string>
#include <memory>

struct Image;

enum EditType {
    PLAMBDA,
    GMIC,
    OCTAVE,
};

std::shared_ptr<Image> edit_images(EditType edittype, const std::string& prog,
                                   const std::vector<std::shared_ptr<Image>>& images);

class ImageCollection* create_edited_collection(EditType edittype, const std::string& prog);

