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

#include "global/global.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_populator/resource_unit_populator.hh"
#include "resource_test_utils.hh"

#include <gtest/gtest.h>
#include <sstream>
#include <string>

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
    WBE::RUPopulator populator;
    std::stringstream ss;
    std::string text_content_storage;
    auto res_chunk = MockResourceChunk::Builder()
        .set_push_data_callback(std::function<WBE::RefRes<char> (size_t, const void *)> (
            [&text_content_storage](size_t p_res_size, const void* p_res) -> WBE::RefRes<char> {
                text_content_storage.resize(p_res_size - 1);
                std::memcpy(text_content_storage.data(), p_res, p_res_size - 1);
                EXPECT_EQ(text_content_storage[p_res_size - 1], '\0');
                return WBE::MEM_NULL;
            }
        ))
        .build(ss);
    populator.populate(*res_chunk, text_scheme);
    EXPECT_EQ(text_content_storage, test_file_content);
}

TEST_F(ResourcePopulatorTest, BinaryReousrceGeneral) {
    // TODO
}

#endif
