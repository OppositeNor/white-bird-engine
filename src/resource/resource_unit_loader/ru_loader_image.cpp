#include "resource/resource_unit_loader/ru_loader_image.h"
#include "core/allocator/allocator.h"
#include "platform/file_system/file_system.h"
#include <stdexcept>

namespace WhiteBirdEngine {

MemID RULoaderImage::load(ResourceChunk& p_chunk, const Path& p_path) {
    std::string ext = FileSystem::get_ext(p_path);
    if (ext == "png") {
        return loader_png.load(p_chunk, p_path);
    }
    if (ext == "ppm") {
        return loader_ppm.load(p_chunk, p_path);
    }
    if (ext == "hdr") {
        return loader_hdr.load(p_chunk, p_path);
    }
    throw std::runtime_error("Image with extension: \"" + ext + "\" is not supported.");
}

}

