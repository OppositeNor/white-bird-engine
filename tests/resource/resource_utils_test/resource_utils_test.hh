// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_RESOURCE_UTILS_TEST_HH__
#define __WBE_RESOURCE_UTILS_TEST_HH__

#include "resource/resource_utils.hh"
#include <gtest/gtest.h>

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
#endif
