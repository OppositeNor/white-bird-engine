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
#ifndef __WBE_RESOURCE_UTILS_TEST_HH__
#define __WBE_RESOURCE_UTILS_TEST_HH__

#include "resource/resource_utils.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "platform/file_system/path.hh"
#include <gtest/gtest.h>
#include <memory>
#include <cstring>

namespace WBE = WhiteBirdEngine;

inline bool test_image_equals(unsigned char* p_img1, unsigned char* p_img2, uint32_t p_size) {
    for (uint32_t i = 0; i < p_size; ++i) {
        if (p_img1[i] != p_img2[i]) {
            return false;
        }
    }
    return true;
}

TEST(ResourceUtilsTest, FlipH4CH) {
    constexpr uint32_t MOCK_IMAGE_WIDTH = 10;
    constexpr uint32_t MOCK_IMAGE_HEIGHT = 3;
    constexpr uint32_t MOCK_IMAGE_CHANNELS = 4;
    unsigned char mock_image[MOCK_IMAGE_HEIGHT * MOCK_IMAGE_WIDTH * MOCK_IMAGE_CHANNELS] = {
        000, 001, 002, 003, 010, 011, 012, 013, 020, 021, 022, 023, 030, 031, 032, 033, 040, 041, 042, 043,
        000, 001, 002, 003, 010, 011, 012, 013, 020, 021, 022, 023, 030, 031, 032, 033, 040, 041, 042, 043,
        100, 101, 102, 103, 110, 111, 112, 113, 120, 121, 122, 123, 130, 131, 132, 133, 140, 141, 142, 143,
        100, 101, 102, 103, 110, 111, 112, 113, 120, 121, 122, 123, 130, 131, 132, 133, 140, 141, 142, 143,
        200, 201, 202, 203, 210, 211, 212, 213, 220, 221, 222, 223, 230, 231, 232, 233, 240, 241, 242, 243,
        200, 201, 202, 203, 210, 211, 212, 213, 220, 221, 222, 223, 230, 231, 232, 233, 240, 241, 242, 243
    };
    unsigned char mock_image_flip_h[MOCK_IMAGE_HEIGHT * MOCK_IMAGE_WIDTH * MOCK_IMAGE_CHANNELS] = {
        040, 041, 042, 043, 030, 031, 032, 033, 020, 021, 022, 023, 010, 011, 012, 013, 000, 001, 002, 003,
        040, 041, 042, 043, 030, 031, 032, 033, 020, 021, 022, 023, 010, 011, 012, 013, 000, 001, 002, 003,
        140, 141, 142, 143, 130, 131, 132, 133, 120, 121, 122, 123, 110, 111, 112, 113, 100, 101, 102, 103,
        140, 141, 142, 143, 130, 131, 132, 133, 120, 121, 122, 123, 110, 111, 112, 113, 100, 101, 102, 103,
        240, 241, 242, 243, 230, 231, 232, 233, 220, 221, 222, 223, 210, 211, 212, 213, 200, 201, 202, 203,
        240, 241, 242, 243, 230, 231, 232, 233, 220, 221, 222, 223, 210, 211, 212, 213, 200, 201, 202, 203
    };
    WBE::img_flip_h(mock_image, MOCK_IMAGE_WIDTH, MOCK_IMAGE_HEIGHT, MOCK_IMAGE_CHANNELS);
    ASSERT_TRUE(test_image_equals(mock_image, mock_image_flip_h, MOCK_IMAGE_WIDTH * MOCK_IMAGE_HEIGHT * MOCK_IMAGE_CHANNELS));
}

TEST(ResourceUtilsTest, FlipV4CH) {
    constexpr uint32_t MOCK_IMAGE_WIDTH = 10;
    constexpr uint32_t MOCK_IMAGE_HEIGHT = 3;
    constexpr uint32_t MOCK_IMAGE_CHANNELS = 4;
    unsigned char mock_image[MOCK_IMAGE_HEIGHT * MOCK_IMAGE_WIDTH * MOCK_IMAGE_CHANNELS] = {
        000, 001, 002, 003, 010, 011, 012, 013, 020, 021, 022, 023, 030, 031, 032, 033, 040, 041, 042, 043,
        000, 001, 002, 003, 010, 011, 012, 013, 020, 021, 022, 023, 030, 031, 032, 033, 040, 041, 042, 043,
        100, 101, 102, 103, 110, 111, 112, 113, 120, 121, 122, 123, 130, 131, 132, 133, 140, 141, 142, 143,
        100, 101, 102, 103, 110, 111, 112, 113, 120, 121, 122, 123, 130, 131, 132, 133, 140, 141, 142, 143,
        200, 201, 202, 203, 210, 211, 212, 213, 220, 221, 222, 223, 230, 231, 232, 233, 240, 241, 242, 243,
        200, 201, 202, 203, 210, 211, 212, 213, 220, 221, 222, 223, 230, 231, 232, 233, 240, 241, 242, 243
    };
    unsigned char mock_image_flip_v[MOCK_IMAGE_HEIGHT * MOCK_IMAGE_WIDTH * MOCK_IMAGE_CHANNELS] = {
        200, 201, 202, 203, 210, 211, 212, 213, 220, 221, 222, 223, 230, 231, 232, 233, 240, 241, 242, 243,
        200, 201, 202, 203, 210, 211, 212, 213, 220, 221, 222, 223, 230, 231, 232, 233, 240, 241, 242, 243,
        100, 101, 102, 103, 110, 111, 112, 113, 120, 121, 122, 123, 130, 131, 132, 133, 140, 141, 142, 143,
        100, 101, 102, 103, 110, 111, 112, 113, 120, 121, 122, 123, 130, 131, 132, 133, 140, 141, 142, 143,
        000, 001, 002, 003, 010, 011, 012, 013, 020, 021, 022, 023, 030, 031, 032, 033, 040, 041, 042, 043,
        000, 001, 002, 003, 010, 011, 012, 013, 020, 021, 022, 023, 030, 031, 032, 033, 040, 041, 042, 043
    };
    WBE::img_flip_v(mock_image, MOCK_IMAGE_WIDTH, MOCK_IMAGE_HEIGHT, MOCK_IMAGE_CHANNELS);
    ASSERT_TRUE(test_image_equals(mock_image, mock_image_flip_v, MOCK_IMAGE_WIDTH * MOCK_IMAGE_HEIGHT * MOCK_IMAGE_CHANNELS));
}

TEST(ResourceUtilsTest, ParseResRelPathSimple) {
    std::string res_path = "textures/player.png";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "player.png");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "textures/");
}

TEST(ResourceUtilsTest, ParseResRelPathNested) {
    std::string res_path = "textures/characters/enemies/goblin.png";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "goblin.png");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "textures/characters/enemies/");
}

TEST(ResourceUtilsTest, ParseResRelPathWithDots) {
    std::string res_path = "textures/../sounds/music/theme.ogg";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "theme.ogg");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "sounds/music/");
}

TEST(ResourceUtilsTest, ParseResRelPathWithCurrentDir) {
    std::string res_path = "textures/./characters/player.png";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "player.png");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "textures/characters/");
}

TEST(ResourceUtilsTest, ParseResRelPathWithMultipleDots) {
    std::string res_path = "textures/characters/../sounds/../music/theme.ogg";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "theme.ogg");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "textures/music/");
}

TEST(ResourceUtilsTest, ParseResRelPathRootFile) {
    std::string res_path = "config.json";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "config.json");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "");
}

TEST(ResourceUtilsTest, ParseResRelPathWithEmptyDirs) {
    std::string res_path = "textures//characters///player.png";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "player.png");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "textures/characters/");
}

TEST(ResourceUtilsTest, ParseResRelPathMoreDotsThanDirs) {
    std::string res_path = "textures/../../../config.json";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "config.json");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "");
}

TEST(ResourceUtilsTest, ParseResRelPathComplexPath) {
    std::string res_path = "assets/./textures/../models/../../sounds/./effects/../music/background.wav";
    WBE::Path result = WBE::parse_res_rel_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "background.wav");
    ASSERT_EQ(static_cast<std::string>(result.get_directory()), "sounds/music/");
}

TEST(ResourceUtilsTest, ImageRotation180) {
    constexpr uint32_t WIDTH = 2;
    constexpr uint32_t HEIGHT = 2;
    constexpr uint32_t CHANNELS = 3;
    
    unsigned char original[WIDTH * HEIGHT * CHANNELS] = {
        1, 2, 3,    4, 5, 6,
        7, 8, 9,    10, 11, 12
    };
    
    unsigned char expected[WIDTH * HEIGHT * CHANNELS] = {
        10, 11, 12,    7, 8, 9,
        4, 5, 6,       1, 2, 3
    };
    
    unsigned char test_image[WIDTH * HEIGHT * CHANNELS];
    std::memcpy(test_image, original, sizeof(original));
    
    WBE::img_rotate_180(test_image, WIDTH, HEIGHT, CHANNELS);
    ASSERT_TRUE(test_image_equals(test_image, expected, WIDTH * HEIGHT * CHANNELS));
}

TEST(ResourceUtilsTest, ImageFlipHorizontal3Channel) {
    constexpr uint32_t WIDTH = 3;
    constexpr uint32_t HEIGHT = 2;
    constexpr uint32_t CHANNELS = 3;
    
    unsigned char original[WIDTH * HEIGHT * CHANNELS] = {
        1, 2, 3,    4, 5, 6,    7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17, 18
    };
    
    unsigned char expected[WIDTH * HEIGHT * CHANNELS] = {
        7, 8, 9,    4, 5, 6,    1, 2, 3,
        16, 17, 18, 13, 14, 15, 10, 11, 12
    };
    
    unsigned char test_image[WIDTH * HEIGHT * CHANNELS];
    std::memcpy(test_image, original, sizeof(original));
    
    WBE::img_flip_h(test_image, WIDTH, HEIGHT, CHANNELS);
    ASSERT_TRUE(test_image_equals(test_image, expected, WIDTH * HEIGHT * CHANNELS));
}

TEST(ResourceUtilsTest, ImageFlipVertical1Channel) {
    constexpr uint32_t WIDTH = 3;
    constexpr uint32_t HEIGHT = 3;
    constexpr uint32_t CHANNELS = 1;
    
    unsigned char original[WIDTH * HEIGHT * CHANNELS] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };
    
    unsigned char expected[WIDTH * HEIGHT * CHANNELS] = {
        7, 8, 9,
        4, 5, 6,
        1, 2, 3
    };
    
    unsigned char test_image[WIDTH * HEIGHT * CHANNELS];
    std::memcpy(test_image, original, sizeof(original));
    
    WBE::img_flip_v(test_image, WIDTH, HEIGHT, CHANNELS);
    ASSERT_TRUE(test_image_equals(test_image, expected, WIDTH * HEIGHT * CHANNELS));
}

TEST(ResourceUtilsTest, ImageFlipHorizontalSinglePixel) {
    constexpr uint32_t WIDTH = 1;
    constexpr uint32_t HEIGHT = 1;
    constexpr uint32_t CHANNELS = 4;
    
    unsigned char original[WIDTH * HEIGHT * CHANNELS] = {255, 128, 64, 32};
    unsigned char expected[WIDTH * HEIGHT * CHANNELS] = {255, 128, 64, 32};
    
    unsigned char test_image[WIDTH * HEIGHT * CHANNELS];
    std::memcpy(test_image, original, sizeof(original));
    
    WBE::img_flip_h(test_image, WIDTH, HEIGHT, CHANNELS);
    ASSERT_TRUE(test_image_equals(test_image, expected, WIDTH * HEIGHT * CHANNELS));
}

TEST(ResourceUtilsTest, ImageFlipVerticalSingleRow) {
    constexpr uint32_t WIDTH = 4;
    constexpr uint32_t HEIGHT = 1;
    constexpr uint32_t CHANNELS = 2;
    
    unsigned char original[WIDTH * HEIGHT * CHANNELS] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char expected[WIDTH * HEIGHT * CHANNELS] = {1, 2, 3, 4, 5, 6, 7, 8};
    
    unsigned char test_image[WIDTH * HEIGHT * CHANNELS];
    std::memcpy(test_image, original, sizeof(original));
    
    WBE::img_flip_v(test_image, WIDTH, HEIGHT, CHANNELS);
    ASSERT_TRUE(test_image_equals(test_image, expected, WIDTH * HEIGHT * CHANNELS));
}

TEST(ResourceUtilsTest, GetResourcePath) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::string res_path = "textures/player.png";
    WBE::Path result = WBE::get_resource_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "player.png");
    std::string dir_str = static_cast<std::string>(result.get_directory());
    ASSERT_TRUE(dir_str.find("textures") != std::string::npos);
}

TEST(ResourceUtilsTest, GetResourcePathNested) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::string res_path = "sounds/music/theme.ogg";
    WBE::Path result = WBE::get_resource_path(res_path);
    
    ASSERT_EQ(result.get_file_name(), "theme.ogg");
    std::string dir_str = static_cast<std::string>(result.get_directory());
    ASSERT_TRUE(dir_str.find("sounds") != std::string::npos);
    ASSERT_TRUE(dir_str.find("music") != std::string::npos);
}

#endif
