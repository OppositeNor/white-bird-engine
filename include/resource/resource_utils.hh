/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef WBE_FILE_RESOURCE_UTILS_HH
#define WBE_FILE_RESOURCE_UTILS_HH

#include "core/engine_core.hh"
#include "platform/file_system/directory.hh"
#include "platform/file_system/file_system.hh"
#include "platform/file_system/path.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace WhiteBirdEngine {
/**
 * @brief Parse a resource path relative to the resource directory.
 *
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
        if (dir_name == "." || dir_name.empty()) {
            continue;
        }
        path_stack.push_back(dir_name);
    }
    return Path(Directory(path_stack, false), file_name);
}

/**
 * @brief Parse a resource path.
 *
 * @param p_res_path The resource path.
 * @return The path to the resource in the filesystem.
 */
inline Path get_resource_path(const std::string& p_res_path) {
    Directory res_directory =
        EngineCore::get_singleton()->file_system->get_resource_directory();
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
void img_flip_h(
    uint8_t* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels);

/**
 * @brief Flip an image vertically.
 *
 * @param p_data The data of the image to be fliped.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channels The channel count of the image.
 */
void img_flip_v(
    uint8_t* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels);

/**
 * @brief Rotate the image clockwise.
 *
 * @param p_data The data of the image.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channel The channel count of the image.
 */
void img_rotate_cw(
    uint8_t* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channel);

/**
 * @brief Rotate the image counter-clockwise.
 *
 * @param p_data The data of the image.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channel The channel count of the image.
 */
void img_rotate_ccw(
    uint8_t* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channel);

/**
 * @brief Rotate the image 180 degrees.
 *
 * @param p_data The data of the image.
 * @param p_width The width of the image.
 * @param p_height The height of the image.
 * @param p_channel The channel count of the image.
 */
void img_rotate_180(
    uint8_t* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channel);

/**
 * @brief Load image data from a image file.
 *
 * @todo Test
 * @param p_path The path to the file.
 * @param p_desired_channels The desired channel count of the image. The image
 * will reformatted according to this, or will not if this value is 0.
 * @param p_flip_h Flip the image horizontally if true.
 * @param p_flip_v Flip the image vertically if true.
 * @param p_width Output: the width of the image
 * @param p_height Output: the height of the image.
 * @param p_file_channels Output: The channel count of the image in its original
 * file.
 * @return The image data.
 */
std::unique_ptr<uint8_t> load_image_data(const Path& p_path,
    uint32_t p_desired_channels,
    bool p_flip_h,
    bool p_flip_v,
    uint32_t& p_width,
    uint32_t& p_height,
    uint32_t& p_file_channels);

/**
 * @brief Load HDR image data from a image file.
 *
 * @todo Test
 * @param p_path The path to the file.
 * @param p_desired_channels The desired channel count of the image. The image
 * will reformatted according to this, or will not if this value is 0.
 * @param p_flip_h Flip the image horizontally if true.
 * @param p_flip_v Flip the image vertically if true.
 * @param p_width Output: the width of the image
 * @param p_height Output: the height of the image.
 * @param p_file_channels Output: The channel count of the image in its original
 * file.
 * @return The image data.
 */
std::unique_ptr<float> load_image_data_hdr(const Path& p_path,
    uint32_t p_desired_channels,
    bool p_flip_h,
    bool p_flip_v,
    uint32_t& p_width,
    uint32_t& p_height,
    uint32_t& p_file_channels);
/**
 * @brief Flip an hdr image horizontally.
 *
 * @param p_data The data of the hdr image to be fliped.
 * @param p_width The width of the hdr image.
 * @param p_height The height of the hdr image.
 * @param p_channels The channel count of the hdr image.
 */
void img_flip_h_hdr(
    float* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels);

/**
 * @brief Flip an hdr image vertically.
 *
 * @param p_data The data of the hdr image to be fliped.
 * @param p_width The width of the hdr image.
 * @param p_height The height of the hdr image.
 * @param p_channels The channel count of the hdr image.
 */
void img_flip_v_hdr(
    float* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels);

/**
 * @brief Rotate the hdr image clockwise.
 *
 * @param p_data The data of the hdr image.
 * @param p_width The width of the hdr image.
 * @param p_height The height of the hdr image.
 * @param p_channel The channel count of the hdr image.
 */
void img_rotate_cw_hdr(
    float* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channel);

/**
 * @brief Rotate the hdr image counter-clockwise.
 *
 * @param p_data The data of the hdr image.
 * @param p_width The width of the hdr image.
 * @param p_height The height of the hdr image.
 * @param p_channel The channel count of the hdr image.
 */
void img_rotate_ccw_hdr(
    float* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channel);

/**
 * @brief Rotate the hdr image 180 degrees.
 *
 * @param p_data The data of the hdr image.
 * @param p_width The width of the hdr image.
 * @param p_height The height of the hdr image.
 * @param p_channel The channel count of the hdr image.
 */
void img_rotate_180_hdr(
    float* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channel);

/**
 * @brief Get the color space from a string.
 *
 * @param p_color_space The string of the color space.
 * @return The according color space.
 */
inline ColorSpace get_color_space_from_str(const std::string& p_color_space) {
    if (p_color_space == "srgb") {
        return ColorSpace::SRGB;
    }
    if (p_color_space == "rgb") {
        return ColorSpace::RGB;
    }
    throw std::runtime_error("Failed to get color space. Color space: \"" +
                             p_color_space + "\" is not supported.");
}

} // namespace WhiteBirdEngine

#endif
