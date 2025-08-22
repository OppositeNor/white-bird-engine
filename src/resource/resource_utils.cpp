#include "resource/resource_utils.hh"
#include "platform/file_system/file_system.hh"
#include "utils/defs.hh"
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

void img_flip_h(unsigned char* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels) {
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

void img_flip_v(unsigned char* p_data, uint32_t p_width, uint32_t p_height, uint32_t p_channels) {
    // Use temporary buffer to increase locality
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

unsigned char* get_pixel(unsigned char* p_data, int p_row, int p_col, int p_width, int p_channel) {
    return p_data + (p_row * p_width + p_col) * p_channel;
}

void sq_img_transpose(unsigned char* p_data, int p_width, int p_height, int p_channel) {
    for (int i = 0; i < p_height; ++i) {
        for (int j = i; j < p_width; ++j) {
            for (int k = 0; k < p_channel; ++k) {
                std::swap(get_pixel(p_data, i, j, p_width, p_channel)[k], get_pixel(p_data, j, i, p_width, p_channel)[k]);
            }
        }
    }
}

void img_rotate_cw(unsigned char* p_data, int p_width, int p_height, int p_channel) {
    if (p_width != p_height) {
        throw std::runtime_error("Only allowed to rotate a square image.");
    }
    sq_img_transpose(p_data, p_width, p_height, p_channel);
    img_flip_h(p_data, p_width, p_height, p_channel);
}

void img_rotate_ccw(unsigned char* p_data, int p_width, int p_height, int p_channel) {
    if (p_width != p_height) {
        throw std::runtime_error("Only allowed to rotate a square image.");
    }
    sq_img_transpose(p_data, p_width, p_height, p_channel);
    img_flip_v(p_data, p_width, p_height, p_channel);
}

void img_rotate_180(unsigned char* p_data, int p_width, int p_height, int p_channel) {
    int image_size = p_width * p_height;
    int half_img_size = image_size << 1;
    for (int i = 0; i < half_img_size; ++i) {
        for (int j = 0; j < p_channel; ++j) {
            std::swap(p_data[i * p_channel + j], p_data[(image_size - i - 1) * p_channel + j]);
        }
    }
}

}
