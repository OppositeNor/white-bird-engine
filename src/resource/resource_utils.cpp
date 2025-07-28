#include "resource/resource_utils.h"
#include "platform/file_system/file_system.h"
#include "utils/defs.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <stb_image.h>
#include <utility>

namespace WhiteBirdEngine {

std::unique_ptr<unsigned char> load_image_data(const Path& p_path, uint32_t p_desired_channels, bool p_flip_h, bool p_flip_v,
                                               uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels) {
    int width, height, file_channels;
    auto result = std::unique_ptr<unsigned char>(
        stbi_load(FileSystem::path_to_string(p_path).c_str(), &width, &height, &file_channels, p_desired_channels));
    if (p_flip_h) {
        if (p_desired_channels != 0) {
            img_flip_h(result.get(), width, height, p_desired_channels);
        }
        else {
            img_flip_h(result.get(), width, height, file_channels);
        }
    }
    if (p_flip_v) {
        if (p_desired_channels != 0) {
            img_flip_v(result.get(), width, height, p_desired_channels);
        }
        else {
            img_flip_v(result.get(), width, height, file_channels);
        }
    }
    WBE_DEBUG_ASSERT(width >= 0 && height >= 0 && file_channels >= 0);
    p_width = width;
    p_height = height;
    p_file_channels = file_channels;
    return result;
}

}
