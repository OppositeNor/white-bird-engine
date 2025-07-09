#include "resource/resource_unit_loader/ru_loader_png.h"
#include "platform/file_system/file_system.h"
#include "utils/defs.h"
#include <cstdint>
#include <stb_image.h>

namespace WhiteBirdEngine {

std::unique_ptr<unsigned char> RULoaderPNG::load_image_data(const Path& p_path, uint32_t p_desired_channels,
                                                        uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels) {
    int width, height, file_channels;
    auto result = std::unique_ptr<unsigned char>(stbi_load(FileSystem::path_to_string(p_path).c_str(), &width, &height, &file_channels, p_desired_channels));
    WBE_DEBUG_ASSERT(width >= 0 && height >= 0 && file_channels >= 0);
    p_width = width;
    p_height = height;
    p_file_channels = file_channels;
    return result;
}

}
