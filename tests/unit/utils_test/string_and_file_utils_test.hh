/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_STRING_AND_FILE_UTILS_TEST_HH
#define WBE_FILE_STRING_AND_FILE_UTILS_TEST_HH
#include "platform/file_system/path.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WBE = WhiteBirdEngine;

namespace {

WBE::Path write_utils_test_file(std::string_view p_name, std::string_view p_content) {
    const WBE::Path path = std::filesystem::temp_directory_path() / p_name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(p_content.data(), static_cast<std::streamsize>(p_content.size()));
    return path;
}

} // namespace

TEST(WBESplitStringTest, SplitsOnEveryToken) {
    EXPECT_EQ(WBE::split_string(std::string("a,bc,d"), ','), (std::vector<std::string>{"a", "bc", "d"}));
    EXPECT_EQ(WBE::split_string(std::string_view("assets/shaders/x"), '/'),
        (std::vector<std::string>{"assets", "shaders", "x"}));
}

TEST(WBESplitStringTest, KeepsEmptyFields) {
    EXPECT_EQ(WBE::split_string(std::string(",a,,b,"), ','), (std::vector<std::string>{"", "a", "", "b", ""}));
}

TEST(WBESplitStringTest, ReturnsWholeStringWithoutToken) {
    EXPECT_EQ(WBE::split_string(std::string("abc"), ','), (std::vector<std::string>{"abc"}));
    EXPECT_EQ(WBE::split_string(std::string(""), ','), (std::vector<std::string>{""}));
}

TEST(WBERemoveHashtagCommentsTest, RemovesCommentsUpToLineEnd) {
    EXPECT_EQ(WBE::remove_hashtag_comments("a = 1 # one\nb = 2\n# full line\nc = 3"), "a = 1 \nb = 2\n\nc = 3");
}

TEST(WBERemoveHashtagCommentsTest, RemovesTrailingCommentWithoutNewline) {
    EXPECT_EQ(WBE::remove_hashtag_comments("value # comment"), "value ");
    EXPECT_EQ(WBE::remove_hashtag_comments("#"), "");
}

TEST(WBERemoveHashtagCommentsTest, KeepsTextWithoutComments) {
    EXPECT_EQ(WBE::remove_hashtag_comments("a\nb\n"), "a\nb\n");
}

TEST(WBEMortonEncodeTest, InterleavesXIntoEvenAndYIntoOddBits) {
    static_assert(WBE::morton_encode(0, 0) == 0);
    EXPECT_EQ(WBE::morton_encode(1, 0), 0b01U);
    EXPECT_EQ(WBE::morton_encode(0, 1), 0b10U);
    EXPECT_EQ(WBE::morton_encode(3, 3), 0b1111U);
    EXPECT_EQ(WBE::morton_encode(0b101, 0b010), 0b011001U);
    EXPECT_EQ(WBE::morton_encode(0xFFFF, 0), 0x55555555U);
    EXPECT_EQ(WBE::morton_encode(0, 0xFFFF), 0xAAAAAAAAU);
}

TEST(WBEMortonEncodeTest, MatchesBitwiseReference) {
    for (uint32_t y = 0; y < 64; ++y) {
        for (uint32_t x = 0; x < 64; ++x) {
            uint32_t expected = 0;
            for (uint32_t bit = 0; bit < 16; ++bit) {
                expected |= ((x >> bit) & 1U) << (2 * bit);
                expected |= ((y >> bit) & 1U) << (2 * bit + 1);
            }
            ASSERT_EQ(WBE::morton_encode(x, y), expected) << x << ", " << y;
        }
    }
}

TEST(WBERingIndexTest, IncrementAndDecrementWrap) {
    static_assert(WBE::ring_increment(0, 4) == 1);
    static_assert(WBE::ring_increment(3, 4) == 0);
    static_assert(WBE::ring_decrement(0, 4) == 3);
    static_assert(WBE::ring_decrement(2, 4) == 1);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(WBE::ring_decrement(WBE::ring_increment(i, 5), 5), i);
    }
}

TEST(WBELoadTextFileTest, LoadsWholeFile) {
    const std::string content = "first line\nsecond line\n";
    const WBE::Path path = write_utils_test_file("wbe_load_text_file_test.txt", content);

    EXPECT_EQ(WBE::load_text_file(path.string().c_str()), content);
    std::filesystem::remove(path);
}

TEST(WBELoadTextFileTest, ThrowsForMissingFile) {
    const WBE::Path path = std::filesystem::temp_directory_path() / "wbe_load_text_file_missing.txt";
    std::filesystem::remove(path);

    EXPECT_THROW((void)WBE::load_text_file(path.string().c_str()), std::runtime_error);
}

TEST(WBELoadBinaryFileTest, LoadsBytes) {
    const std::string content("\x01\x00\x02\xFF", 4);
    const WBE::Path path = write_utils_test_file("wbe_load_binary_file_bytes.bin", content);

    EXPECT_EQ(WBE::load_binary_file(path.string().c_str()), (std::vector<uint8_t>{0x01, 0x00, 0x02, 0xFF}));
    std::filesystem::remove(path);
}

TEST(WBELoadBinaryFileTest, ZeroPadsToElementSize) {
    // Six bytes become two uint32 words; the missing high bytes of the second word are zero.
    const std::string content("\x01\x02\x03\x04\x05\x06", 6);
    const WBE::Path path = write_utils_test_file("wbe_load_binary_file_words.bin", content);

    const std::vector<uint32_t> words = WBE::load_binary_file<uint32_t>(path.string().c_str());

    ASSERT_EQ(words.size(), 2U);
    EXPECT_EQ(words[0], 0x04030201U);
    EXPECT_EQ(words[1], 0x00000605U);
    std::filesystem::remove(path);
}

TEST(WBELoadBinaryFileTest, ThrowsForMissingFile) {
    const WBE::Path path = std::filesystem::temp_directory_path() / "wbe_load_binary_file_missing.bin";
    std::filesystem::remove(path);

    EXPECT_THROW((void)WBE::load_binary_file(path.string().c_str()), std::runtime_error);
}

TEST(WBELoadBinaryFileTest, QueriesSizeThenReadsIntoBuffer) {
    const std::string content = "binary payload";
    const WBE::Path path = write_utils_test_file("wbe_load_binary_file_buffer.bin", content);

    size_t size = 0;
    WBE::load_binary_file(path.string().c_str(), nullptr, &size);
    ASSERT_EQ(size, content.size());
    std::string loaded(size, '\0');
    WBE::load_binary_file(path.string().c_str(), loaded.data(), nullptr);
    EXPECT_EQ(loaded, content);
    std::filesystem::remove(path);
}

TEST(WBELoadBinaryFileTest, ReadsEmptyFileIntoBuffer) {
    const WBE::Path path = write_utils_test_file("wbe_load_binary_file_empty.bin", "");

    size_t size = 1;
    WBE::load_binary_file(path.string().c_str(), nullptr, &size);
    EXPECT_EQ(size, 0U);
    char untouched = 'x';
    WBE::load_binary_file(path.string().c_str(), &untouched, nullptr);
    EXPECT_EQ(untouched, 'x');
    std::filesystem::remove(path);
}

TEST(WBELoadBinaryFileTest, BufferOverloadRejectsInvalidArguments) {
    const WBE::Path path = write_utils_test_file("wbe_load_binary_file_arguments.bin", "x");
    size_t size = 0;

    EXPECT_THROW(WBE::load_binary_file(nullptr, nullptr, &size), std::invalid_argument);
    EXPECT_THROW(WBE::load_binary_file(path.string().c_str(), nullptr, nullptr), std::invalid_argument);
    std::filesystem::remove(path);
    EXPECT_THROW(WBE::load_binary_file(path.string().c_str(), nullptr, &size), std::runtime_error);
}

#endif
