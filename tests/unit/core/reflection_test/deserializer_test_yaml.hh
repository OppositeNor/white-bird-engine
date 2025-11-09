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
#ifndef __DESERIALIZER_TEST_YAML_HH__
#define __DESERIALIZER_TEST_YAML_HH__

#include "core/parser/parser_yaml.hh"
#include "reflection_test_data.hh"
#include "generated/serializables_sd.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "utils/utils.hh"
#include <glm/glm.hpp>
#include <string>
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

class WBEDeserializerYAMLTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    std::unique_ptr<WBE::Global> global;
    std::string test_serialize_yaml_general =
    R"(si32_test: 3
si64_test: -62
ui32_test: 42
ui64_test: 59
f32_test: 3.14
f64_test: 2.718
vec3_test:
  x: 1
  y: -2
  z: 3
vec4_test:
  x: 1
  y: -2
  z: 3
  w: -4
str_test: "Hello!"
buffer_test: "how are you?"
)";
    std::string test_serialize_yaml_zeros =
    R"(si32_test: 0
si64_test: 0
ui32_test: 0
ui64_test: 0
f32_test: 0
f64_test: 0
vec3_test:
  x: 0
  y: 0
  z: 0
vec4_test:
  x: 0
  y: 0
  z: 0
  w: 0
str_test: ""
buffer_test: ""
)";
    std::string test_serialize_yaml_nesting =
    R"(nesting_id: 7
name: "parent"
small_buffer: "abc"
nested_test:
  si32_test: 3
  si64_test: -62
  ui32_test: 42
  ui64_test: 59
  f32_test: 3.14
  f64_test: 2.718
  vec3_test:
    x: 1
    y: -2
    z: 3
  vec4_test:
    x: 1
    y: -2
    z: 3
    w: -4
  str_test: "Hello!"
  buffer_test: "how are you?"
)";
};

TEST_F(WBEDeserializerYAMLTest, General) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_general);
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

TEST_F(WBEDeserializerYAMLTest, ZerosAndStrings) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_zeros);
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

TEST_F(WBEDeserializerYAMLTest, StringsAndBufferContent) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_general);
    sd.deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, std::string("Hello!"));
    EXPECT_STREQ(test_obj.buffer_test.buffer, "how are you?");
}

TEST_F(WBEDeserializerYAMLTest, BufferTooLongThrows) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Create YAML where buffer_test is longer than Buffer<16>
    std::string long_buffer_yaml = "buffer_test: \"";
    long_buffer_yaml += std::string(32, 'A');
    long_buffer_yaml += "\"\n";

    parser.parse_from_buffer(long_buffer_yaml);
    ASSERT_THROW(sd.deserialize(parser.get_data(), test_obj), std::runtime_error);
}

TEST_F(WBEDeserializerYAMLTest, PartialUpdateOnlyOneField) {
    WBE::SerializableSD<TestSerializable> sd;
    TestSerializable test_obj;
    // Initialize with sentinel values
    test_obj.si32_test = 100;
    test_obj.si64_test = 200;
    test_obj.ui32_test = 300;
    test_obj.str_test = "orig";

    WBE::ParserYAML parser;
    // YAML contains only si32_test
    parser.parse_from_buffer("si32_test: -7\n");
    sd.deserialize(parser.get_data(), test_obj);

    // Updated
    EXPECT_EQ(test_obj.si32_test, -7);
    // Others unchanged
    EXPECT_EQ(test_obj.si64_test, 200);
    EXPECT_EQ(test_obj.ui32_test, 300u);
    EXPECT_EQ(test_obj.str_test, std::string("orig"));
}

TEST_F(WBEDeserializerYAMLTest, NestingGeneral) {
    WBE::SerializableSD<TestSerializableNesting> sd;
    TestSerializableNesting test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_nesting);
    sd.deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.nesting_id, 7);
    EXPECT_EQ(test_obj.name, std::string("parent"));
    EXPECT_STREQ(test_obj.small_buffer.buffer, "abc");

    // Check nested_test values (matches test_serialize_yaml_general)
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
