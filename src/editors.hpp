#pragma once

#include <vector>
#include <string>
#include <memory>

struct Image;
struct Chunk;

enum EditType {
    PLAMBDA,
    GMIC,
    OCTAVE,
};

std::shared_ptr<Image> edit_images(EditType edittype, const std::string& prog,
                                   const std::vector<std::shared_ptr<Image>>& images,
                                   std::string& error);

std::shared_ptr<Chunk> edit_chunks(EditType edittype, const std::string& _prog,
                                   const std::vector<std::shared_ptr<Chunk>>& chunks,
                                   std::string& error);

std::shared_ptr<class ImageCollection> create_edited_collection(EditType edittype, const std::string& prog);

