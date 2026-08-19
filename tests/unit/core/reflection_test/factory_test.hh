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
#ifndef WBE_FILE_SERIALIZABLE_FACTORY_TEST_HH
#define WBE_FILE_SERIALIZABLE_FACTORY_TEST_HH

#include "core/parser/parser_json.hh"
#include "generated/serializable_factory.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "reflection_test_data.hh"

#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

namespace WBE = WhiteBirdEngine;

class WBESerializableFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
};

TEST_F(WBESerializableFactoryTest, CreatesSerializableObjectByName) {
    WBE::ParserJSON parser;
    parser.parse_from_buffer(R"({"si32_test":3,"str_test":"Hello!","optional_str_test":"Optional hello!"})");

    void* created = WBE::SerializableFactory::create_serializable_object("TestSerializable", parser.get_data());
    auto* serializable = static_cast<WBE::TestSerializable*>(created);

    ASSERT_NE(serializable, nullptr);
    EXPECT_EQ(serializable->si32_test, 3);
    EXPECT_EQ(serializable->str_test, "Hello!");
    ASSERT_TRUE(serializable->optional_str_test.has_value());
    EXPECT_EQ(serializable->optional_str_test.value(), "Optional hello!");

    serializable->~TestSerializable();
    ::operator delete(created, std::align_val_t(alignof(WBE::TestSerializable)));
}

TEST_F(WBESerializableFactoryTest, SupportsInPlaceConstruction) {
    WBE::ParserJSON parser;
    parser.parse_from_buffer(R"({"si32_test":9,"ui32_test":42,"str_test":"In place"})");

    alignas(WBE::TestSerializable) std::byte storage[sizeof(WBE::TestSerializable)];
    void* created = WBE::SerializableFactory::create_serializable_object("TestSerializable", parser.get_data(), storage);
    auto* serializable = static_cast<WBE::TestSerializable*>(created);

    ASSERT_EQ(created, static_cast<void*>(storage));
    EXPECT_EQ(serializable->si32_test, 9);
    EXPECT_EQ(serializable->ui32_test, 42U);
    EXPECT_EQ(serializable->str_test, "In place");

    serializable->~TestSerializable();
}

TEST_F(WBESerializableFactoryTest, ThrowsForUnknownSerializableName) {
    WBE::ParserJSON parser;
    parser.parse_from_buffer(R"({})");

    EXPECT_THROW(WBE::SerializableFactory::create_serializable_object("MissingSerializable", parser.get_data()), std::runtime_error);
}

#endif