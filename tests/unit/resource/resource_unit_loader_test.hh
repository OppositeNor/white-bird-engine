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
#ifndef WBE_FILE_RESOURCE_POPULATOR_TEST_HH
#define WBE_FILE_RESOURCE_POPULATOR_TEST_HH

#include "core/allocator/i_allocator.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource_test_utils.hh"
#include "resource/resource_utils.hh"
#include "utils/utils.hh"

#include <cstring>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace WBE = WhiteBirdEngine;

class ResourcePopulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
        text_scheme.path = "test_text_files/test_file_utf8.txt";
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
    WBE::RUSchemeText text_scheme;
    std::string test_file_content = R"txt(这是一个测试文本。
English text is also supported.
日本語テキストもサポート。
한국어 텍스트도 지원됩니다.
Текст на русском тоже поддерживается.
Le texte français est également pris en charge.
El texto en español también está soportado.
Der deutsche Text wird ebenfalls unterstützt.
Il testo in italiano è supportato.
Português também é suportado.
النص العربي مدعوم أيضًا.
טקסט בעברית נתמך גם כן.
हिंदी पाठ भी समर्थित है।
ภาษาไทยก็รองรับเช่นกัน。
ქართული ტექსტიც მხარდაჭერილია.
Ελληνικό κείμενο υποστηρίζεται επίσης.
Türkçe metin de desteklenmektedir.
Український текст також підтримується.
Tiếng Việt cũng được hỗ trợ.
Emoji 测试: 😃🚀✨🌍
数学字符: ∀x∈ℝ, x² ≥ 0
组合字符: á é í ó ú
)txt";
};

TEST_F(ResourcePopulatorTest, TextResourceGeneral) {
    WBE::ResourceUnitLoaderText loader;
    std::stringstream ss;
    std::string text_content_storage;
    auto res_chunk = MockResourceChunk::Builder()
        .set_push_data_callback(std::function<WBE::RefRes<char> (size_t, const void *)> (
            [&text_content_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                text_content_storage.resize(p_res_size - 1);
                std::memcpy(text_content_storage.data(), p_res, p_res_size - 1);
                EXPECT_EQ(static_cast<const char*>(p_res)[p_res_size - 1], '\0');
                return WBE::MEM_NULL;
            }
        ))
        .build(ss);
    loader.populate_scheme(*res_chunk, text_scheme);
    EXPECT_EQ(text_content_storage, test_file_content);
}

TEST_F(ResourcePopulatorTest, BinaryReousrceGeneral) {
    WBE::ResourceUnitLoaderBinary loader;
    std::stringstream ss;
    std::vector<char> binary_data_storage;
    WBE::RUBinary captured_ru_binary;
    WBE::RUSchemeBinary binary_scheme;
    binary_scheme.path = "test_text_files/test_file_utf8.txt";
    auto res_chunk = MockResourceChunk::Builder()
        .set_push_data_callback(std::function<WBE::RefRes<char> (size_t, const void *)> (
            [&binary_data_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                binary_data_storage.resize(p_res_size);
                std::memcpy(binary_data_storage.data(), p_res, p_res_size);
                return WBE::MEM_NULL;
            }
        ))
        .set_push_res_unit_ptr_callback(std::function<WBE::RefRes<char> (const std::string&, const void*, size_t)> (
            [&captured_ru_binary](const std::string&, const void* p_res, size_t p_size) -> WBE::RefRes<char> {
                EXPECT_EQ(p_size, sizeof(WBE::RUBinary));
                std::memcpy(static_cast<void*>(&captured_ru_binary), p_res, p_size);
                return WBE::MEM_NULL;
            }
        ))
        .build(ss);

    loader.populate_scheme(*res_chunk, binary_scheme);

    auto expected_binary = WBE::load_binary_file(
        static_cast<std::string>(WBE::get_resource_path(binary_scheme.path)).c_str());
    EXPECT_EQ(captured_ru_binary.size, expected_binary.size());
    EXPECT_EQ(binary_data_storage.size(), expected_binary.size());
    EXPECT_EQ(binary_data_storage, expected_binary);
}

TEST_F(ResourcePopulatorTest, ImageResourceGeneral) {
    WBE::ResourceUnitLoaderImage loader;
    std::stringstream ss;
    std::vector<char> image_data_storage;
    WBE::RUImage captured_ru_image;
    WBE::RUSchemeImage image_scheme;
    image_scheme.path = "assets/images/test_ppm3.ppm";
    image_scheme.color_space = "srgb";
    auto res_chunk = MockResourceChunk::Builder()
        .set_push_data_callback(std::function<WBE::RefRes<char> (size_t, const void *)> (
            [&image_data_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                image_data_storage.resize(p_res_size);
                std::memcpy(image_data_storage.data(), p_res, p_res_size);
                return WBE::MEM_NULL;
            }
        ))
        .set_push_res_unit_ptr_callback(std::function<WBE::RefRes<char> (const std::string&, const void*, size_t)> (
            [&captured_ru_image](const std::string&, const void* p_res, size_t p_size) -> WBE::RefRes<char> {
                EXPECT_EQ(p_size, sizeof(WBE::RUImage));
                std::memcpy(static_cast<void*>(&captured_ru_image), p_res, p_size);
                return WBE::MEM_NULL;
            }
        ))
        .build(ss);
    loader.populate_scheme(*res_chunk, image_scheme);
    EXPECT_EQ(captured_ru_image.width, 16);
    EXPECT_EQ(captured_ru_image.height, 16);
    EXPECT_EQ(captured_ru_image.channel_count, 3);
    EXPECT_EQ(captured_ru_image.color_space, WBE::get_color_space_from_str("srgb"));
    EXPECT_EQ(image_data_storage.size(), 16 * 16 * 3);
    // Check first pixel: should be red (255, 0, 0)
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(0)), 255);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(1)), 0);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(2)), 0);
    // Check second pixel: should be red (255, 0, 0)
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(3)), 255);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(4)), 0);
    EXPECT_EQ(static_cast<unsigned char>(image_data_storage.at(5)), 0);
}

TEST_F(ResourcePopulatorTest, AudioResourceGeneral) {
    // TODO: loader is not implemented yet.
}

TEST_F(ResourcePopulatorTest, FontResourceGeneral) {
    // TODO: loader is not implemented yet.
}

TEST_F(ResourcePopulatorTest, MaterialResourceGeneral) {
    // TODO: loader is not implemented yet.
}

TEST_F(ResourcePopulatorTest, VideoResourceGeneral) {
    // TODO: loader is not implemented yet.
}

#endif
