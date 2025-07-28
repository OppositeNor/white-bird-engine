#ifndef __WBE_RESOURCE_UTILS_H__
#define __WBE_RESOURCE_UTILS_H__

#include "core/engine_core.h"
#include "platform/file_system/directory.h"
#include "platform/file_system/file_system.h"
#include "platform/file_system/path.h"
#include <string>
namespace WhiteBirdEngine {

/**
 * @brief Parse a resource path relative to the resource directory.
 *
 * @todo Test
 * @param p_res_path The resource path.
 * @return The resource path relative to the resource directory.
 */
inline Path parse_res_rel_path(const std::string& p_res_path) {
    auto splitted = split_string(p_res_path, '/');
    std::string file_name = splitted.back();
    splitted.pop_back();
    std::vector<std::string> path_stack;
    path_stack.reserve(splitted.size());
    for (auto& dir_name : splitted) {
        if (dir_name == "..") {
            if (!path_stack.empty()) {
                path_stack.pop_back();
            }
            continue;
        }
        if (dir_name == "." || dir_name == "") {
            continue;
        }
        path_stack.push_back(dir_name);
    }
    return Path(Directory(path_stack, false), file_name);
}

/**
 * @brief Parse a resource path.
 *
 * @todo Test
 * @param p_res_path The resource path.
 * @return The path to the resource in the filesystem.
 */
inline Path get_resource_path(const std::string& p_res_path) {
    Directory res_directory = EngineCore::get_singleton()->file_system->get_resource_directory();
    return combine(res_directory, parse_res_rel_path(p_res_path));
}

/**
 * @brief Flip an image horizontally.
 *
 * @param p_data The data of the image to be fliped.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channels The channel count of the image.
 */
void img_flip_h(unsigned char* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels);

/**
 * @brief Flip an image vertically.
 *
 * @param p_data The data of the image to be fliped.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channels The channel count of the image.
 */
void img_flip_v(unsigned char* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels);

/**
 * @brief Load image data from a image file.
 *
 * @todo Test
 * @param p_path The path to the file.
 * @param p_desired_channels The desired channel count of the image. The image will reformatted according to
 * this, or will not if this value is 0.
 * @param p_flip_h Flip the image horizontally if true.
 * @param p_flip_v Flip the image vertically if true.
 * @param p_width Output: the width of the image
 * @param p_height Output: the height of the image.
 * @param p_file_channels Output: The channel count of the image in its original file.
 * @return The image data.
 */
std::unique_ptr<unsigned char> load_image_data(const Path& p_path, uint32_t p_desired_channels, bool p_flip_h, bool p_flip_v,
                                               uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels);

/**
 * @brief Get the color space from a string.
 *
 * @param p_color_space The string of the color space.
 * @return The according color space.
 */
inline ColorSpace get_color_space(const std::string& p_color_space) {
    if (p_color_space == "srgb") {
        return ColorSpace::SRGB;
    }
    if (p_color_space == "rgb") {
        return ColorSpace::RGB;
    }
    throw std::runtime_error("Failed to get color space. Color space: \"" + p_color_space + "\" is not supported.");
}

/**
 * @brief Flip an image horizontally.
 *
 * @todo Test
 * @param p_data The data of the image.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channels The channel count of the image.
 */
inline void img_flip_h(unsigned char* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels) {
    uint32_t row_size = p_width * p_channels;
    for (uint32_t i = 0; i < p_height; ++i) {
        for (uint32_t j = 0; j < p_width / 2; ++j) {
            for (uint32_t k = 0; k < p_channels; ++k) {
                std::swap(p_data[i * row_size + j * p_channels + k],
                          p_data[i * row_size + (p_width - j - 1) * p_channels + k]);
            }
        }
    }
}

/**
 * @brief Flip an image vertically.
 *
 * @todo Test
 * @param p_data The data of the image.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channels The channel count of the image.
 */
inline void img_flip_v(unsigned char* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels) {
    // Use temporary buffer to increase cache hits.
    uint32_t row_size = p_width * p_channels;
    std::unique_ptr<unsigned char[]> temp = std::make_unique<unsigned char[]>(row_size);
    unsigned char* source_row;
    unsigned char* target_row;
    for (uint32_t i = 0; i < p_height / 2; ++i) {
        source_row = p_data + i * row_size;
        target_row = p_data + (p_height - i - 1) * row_size;
        memcpy(temp.get(), target_row, row_size);
        memcpy(target_row, source_row, row_size);
        memcpy(source_row, temp.get(), row_size);
    }
}

}

#endif
