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
#ifndef __DESERIALIZER_TEST_JSON_HH__
#define __DESERIALIZER_TEST_JSON_HH__

#include "core/parser/parser_json.hh"
#include "reflection_test_data.hh"
#include "generated/serializables_sd.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "utils/utils.hh"
#include <glm/glm.hpp>
#include <string>
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

class WBEDeserializerJSONTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
    std::string test_serialize_json_general = 
    R"({)"
    R"("si32_test" : 3,)"
    R"("si64_test" : -62,)"
    R"("ui32_test" : 42,)"
    R"("ui64_test" : 59,)"
    R"("f32_test" : 3.14,)"
    R"("f64_test" : 2.718,)"
    R"("vec3_test" : {"x": 1, "y": -2, "z": 3},)"
    R"("vec4_test" : {"x": 1, "y": -2, "z": 3, "w": -4},)"
    R"("str_test" : "Hello!",)"
    R"("buffer_test" : "how are you?")"
    R"(})";
    std::string test_serialize_json_zeros = 
    R"({)"
    R"("si32_test" : 0,)"
    R"("si64_test" : 0,)"
    R"("ui32_test" : 0,)"
    R"("ui64_test" : 0,)"
    R"("f32_test" : 0,)"
    R"("f64_test" : 0,)"
    R"("vec3_test" : {"x": 0, "y": 0, "z": 0},)"
    R"("vec4_test" : {"x": 0, "y": 0, "z": 0, "w": 0},)"
    R"("str_test" : "",)"
    R"("buffer_test" : "")"
    R"(})";
    std::string test_serialize_json_nesting =
    R"({)"
    R"("nesting_id" : 7,)"
    R"("name" : "parent",)"
    R"("small_buffer" : "abc",)"
    R"("nested_test" : {)"
    R"("si32_test" : 3,)"
    R"("si64_test" : -62,)"
    R"("ui32_test" : 42,)"
    R"("ui64_test" : 59,)"
    R"("f32_test" : 3.14,)"
    R"("f64_test" : 2.718,)"
    R"("vec3_test" : {"x": 1, "y": -2, "z": 3},)"
    R"("vec4_test" : {"x": 1, "y": -2, "z": 3, "w": -4},)"
    R"("str_test" : "Hello!",)"
    R"("buffer_test" : "how are you?")"
    R"(})"
    R"(})";
};

TEST_F(WBEDeserializerJSONTest, Empty) {
    WBE::SerializableSD<TestEmptySerializable> sd;
    TestEmptySerializable test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer("{}");
    sd.deserialize(parser.get_data(), test_obj);
}

TEST_F(WBEDeserializerJSONTest, General) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer(test_serialize_json_general);
    sd.deserialize(parser.get_data(), test_obj);
    EXPECT_EQ(test_obj.si32_test, 3);
    EXPECT_EQ(test_obj.si64_test, -62);
    EXPECT_EQ(test_obj.ui32_test, 42);
    EXPECT_EQ(test_obj.ui64_test, 59);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 3.14);
    EXPECT_FLOAT_EQ(test_obj.f64_test, 2.718);
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(1, -2, 3));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(1, -2, 3, -4));
    EXPECT_EQ(test_obj.str_test, "Hello!");
    EXPECT_TRUE(strcmp(test_obj.buffer_test.buffer, "how are you?") == 0);
}

TEST_F(WBEDeserializerJSONTest, ZerosAndStrings) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer(test_serialize_json_zeros);
    sd.deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.si32_test, 0);
    EXPECT_EQ(test_obj.si64_test, 0);
    EXPECT_EQ(test_obj.ui32_test, 0u);
    EXPECT_EQ(test_obj.ui64_test, 0u);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 0.0f);
    EXPECT_FLOAT_EQ(test_obj.f64_test, 0.0);
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(0,0,0));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(0,0,0,0));
    EXPECT_EQ(test_obj.str_test, std::string(""));
    EXPECT_STREQ(test_obj.buffer_test.buffer, "");
}

TEST_F(WBEDeserializerJSONTest, StringsAndBufferContent) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer(test_serialize_json_general);
    sd.deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, std::string("Hello!"));
    EXPECT_STREQ(test_obj.buffer_test.buffer, "how are you?");
}

TEST_F(WBEDeserializerJSONTest, BufferTooLongThrows) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserJSON parser;

    // Create JSON where buffer_test is longer than Buffer<16>
    std::string long_buffer_json = R"({ "buffer_test": ")";
    long_buffer_json += std::string(32, 'A');
    long_buffer_json += R"(" })";

    parser.parse_from_buffer(long_buffer_json);
    ASSERT_THROW(sd.deserialize(parser.get_data(), test_obj), std::runtime_error);
}

TEST_F(WBEDeserializerJSONTest, PartialUpdateOnlyOneField) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    // Initialize with sentinel values
    test_obj.si32_test = 100;
    test_obj.si64_test = 200;
    test_obj.ui32_test = 300;
    test_obj.str_test = "orig";

    WBE::ParserJSON parser;
    // JSON contains only si32_test
    parser.parse_from_buffer(R"({ "si32_test": -7 })");
    sd.deserialize(parser.get_data(), test_obj);

    // Updated
    EXPECT_EQ(test_obj.si32_test, -7);
    // Others unchanged
    EXPECT_EQ(test_obj.si64_test, 200);
    EXPECT_EQ(test_obj.ui32_test, 300u);
    EXPECT_EQ(test_obj.str_test, std::string("orig"));
}

TEST_F(WBEDeserializerJSONTest, NestingGeneral) {
    WBE::SerializableSD<TestSerializableNesting> sd;
    TestSerializableNesting test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer(test_serialize_json_nesting);
    sd.deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.nesting_id, 7);
    EXPECT_EQ(test_obj.name, std::string("parent"));
    EXPECT_STREQ(test_obj.small_buffer.buffer, "abc");

    // Check nested_test values (matches test_serialize_json_general)
    EXPECT_EQ(test_obj.nested_test.si32_test, 3);
    EXPECT_EQ(test_obj.nested_test.si64_test, -62);
    EXPECT_EQ(test_obj.nested_test.ui32_test, 42u);
    EXPECT_EQ(test_obj.nested_test.ui64_test, 59u);
    EXPECT_FLOAT_EQ(test_obj.nested_test.f32_test, 3.14f);
    EXPECT_FLOAT_EQ(test_obj.nested_test.f64_test, 2.718);
    EXPECT_EQ(test_obj.nested_test.str_test, "Hello!");
    EXPECT_STREQ(test_obj.nested_test.buffer_test.buffer, "how are you?");

}

#endif
