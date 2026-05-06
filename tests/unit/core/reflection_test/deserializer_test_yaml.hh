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
#include "generated/serializables_sd.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "reflection_test_data.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <cstring>
#include <exception>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

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
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_general);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);
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
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_zeros);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.si32_test, 0);
    EXPECT_EQ(test_obj.si64_test, 0);
    EXPECT_EQ(test_obj.ui32_test, 0U);
    EXPECT_EQ(test_obj.ui64_test, 0U);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 0.0F);
    EXPECT_FLOAT_EQ(test_obj.f64_test, 0.0);
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(0, 0, 0));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(0, 0, 0, 0));
    EXPECT_EQ(test_obj.str_test, std::string(""));
    EXPECT_STREQ(test_obj.buffer_test.buffer, "");
}

TEST_F(WBEDeserializerYAMLTest, StringsAndBufferContent) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_general);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, std::string("Hello!"));
    EXPECT_STREQ(test_obj.buffer_test.buffer, "how are you?");
}

TEST_F(WBEDeserializerYAMLTest, BufferTooLongThrows) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Create YAML where buffer_test is longer than Buffer<16>
    std::string long_buffer_yaml = "buffer_test: \"";
    long_buffer_yaml += std::string(32, 'A');
    long_buffer_yaml += "\"\n";

    parser.parse_from_buffer(long_buffer_yaml);
    ASSERT_THROW(sd.deserialize(parser.get_data(), test_obj), std::runtime_error);
}

TEST_F(WBEDeserializerYAMLTest, PartialUpdateOnlyOneField) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    // Initialize with sentinel values
    test_obj.si32_test = 100;
    test_obj.si64_test = 200;
    test_obj.ui32_test = 300;
    test_obj.str_test = "orig";

    WBE::ParserYAML parser;
    // YAML contains only si32_test
    parser.parse_from_buffer("si32_test: -7\n");
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    // Updated
    EXPECT_EQ(test_obj.si32_test, -7);
    // Others unchanged
    EXPECT_EQ(test_obj.si64_test, 200);
    EXPECT_EQ(test_obj.ui32_test, 300U);
    EXPECT_EQ(test_obj.str_test, std::string("orig"));
}

TEST_F(WBEDeserializerYAMLTest, NestingGeneral) {
    WBE::SerializableSD<WBE::TestSerializableNesting> sd;
    WBE::TestSerializableNesting test_obj;
    WBE::ParserYAML parser;
    parser.parse_from_buffer(test_serialize_yaml_nesting);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializableNesting>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.nesting_id, 7);
    EXPECT_EQ(test_obj.name, std::string("parent"));
    EXPECT_STREQ(test_obj.small_buffer.buffer, "abc");

    // Check nested_test values (matches test_serialize_yaml_general)
    EXPECT_EQ(test_obj.nested_test.si32_test, 3);
    EXPECT_EQ(test_obj.nested_test.si64_test, -62);
    EXPECT_EQ(test_obj.nested_test.ui32_test, 42U);
    EXPECT_EQ(test_obj.nested_test.ui64_test, 59U);
    EXPECT_FLOAT_EQ(test_obj.nested_test.f32_test, 3.14F);
    EXPECT_FLOAT_EQ(test_obj.nested_test.f64_test, 2.718);
    EXPECT_EQ(test_obj.nested_test.str_test, "Hello!");
    EXPECT_STREQ(test_obj.nested_test.buffer_test.buffer, "how are you?");
}

// Edge case tests
TEST_F(WBEDeserializerYAMLTest, EdgeCase_MalformedYAML) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test malformed YAML
    EXPECT_THROW(parser.parse_from_buffer("key: value\n  bad_indent: broken"), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_EmptyYAML) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Initialize with sentinel values to verify no changes
    test_obj.si32_test = 999;
    test_obj.str_test = "unchanged";

    parser.parse_from_buffer("");
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    // Values should remain unchanged when not present in YAML
    EXPECT_EQ(test_obj.si32_test, 999);
    EXPECT_EQ(test_obj.str_test, "unchanged");
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_NullValues_ShouldFail) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test YAML with null values for non-pointer types - should fail
    std::string null_yaml = R"(si32_test: null
str_test: null
buffer_test: null
)";

    parser.parse_from_buffer(null_yaml);
    // Should throw exception since non-pointer types cannot be null
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_NullValues_IndividualFields) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test null value for integer field
    std::string null_int_yaml = "si32_test: null\n";
    parser.parse_from_buffer(null_int_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);

    // Test null value for float field
    std::string null_float_yaml = "f32_test: null\n";
    parser.parse_from_buffer(null_float_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);

    // Test null value for vector field
    std::string null_vector_yaml = "vec3_test: null\n";
    parser.parse_from_buffer(null_vector_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_TypeMismatch_ShouldFail) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test type mismatches - should fail
    std::string mismatch_yaml = R"(si32_test: "not_a_number"
str_test: 12345
)";

    parser.parse_from_buffer(mismatch_yaml);
    // Should throw exception for type mismatches
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_TypeMismatch_IndividualTypes) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test string where integer expected
    std::string int_mismatch_yaml = "si32_test: \"not_an_integer\"\n";
    parser.parse_from_buffer(int_mismatch_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_ExtremeValues) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    std::string extreme_yaml = R"(si32_test: 2147483647
si64_test: -9223372036854775808
ui32_test: 4294967295
ui64_test: 18446744073709551615
f32_test: 3.4028235e+38
f64_test: -1.7976931348623157e+308
)";

    parser.parse_from_buffer(extreme_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.si32_test, std::numeric_limits<int32_t>::max());
    EXPECT_EQ(test_obj.si64_test, std::numeric_limits<int64_t>::min());
    EXPECT_EQ(test_obj.ui32_test, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(test_obj.ui64_test, std::numeric_limits<uint64_t>::max());
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_UnicodeStrings) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    std::string unicode_yaml = "str_test: \"Hello 世界 🌍 ñáéíóú\"\n";

    parser.parse_from_buffer(unicode_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, "Hello 世界 🌍 ñáéíóú");
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_LongStringsAndBuffers) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test very long string
    std::string long_string(1000, 'A');
    std::string long_string_yaml = "str_test: \"" + long_string + "\"\n";

    parser.parse_from_buffer(long_string_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, long_string);
}

TEST_F(WBEDeserializerYAMLTest, EdgeCase_EmptyStringAndBuffer) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    std::string empty_yaml = R"(str_test: ""
buffer_test: ""
)";

    parser.parse_from_buffer(empty_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, "");
    EXPECT_STREQ(test_obj.buffer_test.buffer, "");
}

// Vector container tests
TEST_F(WBEDeserializerYAMLTest, VectorContainer_EmptyVectors) {
    WBE::SerializableSD<WBE::TestVectorContainer> sd;
    WBE::TestVectorContainer test_obj;
    WBE::ParserYAML parser;

    std::string empty_vectors_yaml = R"(ints: []
strs: []
children: []
)";

    parser.parse_from_buffer(empty_vectors_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestVectorContainer>::deserialize(parser.get_data(), test_obj);

    EXPECT_TRUE(test_obj.ints.empty());
    EXPECT_TRUE(test_obj.strs.empty());
    EXPECT_TRUE(test_obj.children.empty());
}

TEST_F(WBEDeserializerYAMLTest, VectorContainer_PopulatedVectors) {
    WBE::SerializableSD<WBE::TestVectorContainer> sd;
    WBE::TestVectorContainer test_obj;
    WBE::ParserYAML parser;

    std::string vectors_yaml = R"(ints:
  - 1
  - 2
  - 3
  - -4
  - 0
strs:
  - "hello"
  - "world"
  - ""
  - "test"
children:
  - si32_test: 10
    str_test: "child1"
  - si32_test: 20
    str_test: "child2"
)";

    parser.parse_from_buffer(vectors_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestVectorContainer>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.ints.size(), 5);
    EXPECT_EQ(test_obj.ints[0], 1);
    EXPECT_EQ(test_obj.ints[3], -4);
    EXPECT_EQ(test_obj.ints[4], 0);

    EXPECT_EQ(test_obj.strs.size(), 4);
    EXPECT_EQ(test_obj.strs[0], "hello");
    EXPECT_EQ(test_obj.strs[2], "");

    EXPECT_EQ(test_obj.children.size(), 2);
    EXPECT_EQ(test_obj.children[0].si32_test, 10);
    EXPECT_EQ(test_obj.children[0].str_test, "child1");
    EXPECT_EQ(test_obj.children[1].si32_test, 20);
    EXPECT_EQ(test_obj.children[1].str_test, "child2");
}

// Deep nesting tests
TEST_F(WBEDeserializerYAMLTest, DeepNesting_Depth2) {
    WBE::SerializableSD<WBE::TestDepth2> sd;
    WBE::TestDepth2 test_obj;
    WBE::ParserYAML parser;

    std::string depth2_yaml = R"(depth2_id: 100
depth2_name: "depth2_test"
nested:
  si32_test: 42
  str_test: "nested_string"
  vec3_test:
    x: 1.0
    y: 2.0
    z: 3.0
)";

    parser.parse_from_buffer(depth2_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestDepth2>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.depth2_id, 100);
    EXPECT_EQ(test_obj.depth2_name, "depth2_test");
    EXPECT_EQ(test_obj.nested.si32_test, 42);
    EXPECT_EQ(test_obj.nested.str_test, "nested_string");
    EXPECT_EQ(test_obj.nested.vec3_test, glm::vec3(1.0F, 2.0F, 3.0F));
}

TEST_F(WBEDeserializerYAMLTest, DeepNesting_Depth3) {
    WBE::SerializableSD<WBE::TestDepth3> sd;
    WBE::TestDepth3 test_obj;
    WBE::ParserYAML parser;

    std::string depth3_yaml = R"(depth3_id: 200
depth3_name: "depth3_test"
nested2:
  depth2_id: 100
  depth2_name: "depth2_nested"
  nested:
    si32_test: 42
    str_test: "deeply_nested"
)";

    parser.parse_from_buffer(depth3_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestDepth3>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.depth3_id, 200);
    EXPECT_EQ(test_obj.depth3_name, "depth3_test");
    EXPECT_EQ(test_obj.nested2.depth2_id, 100);
    EXPECT_EQ(test_obj.nested2.depth2_name, "depth2_nested");
    EXPECT_EQ(test_obj.nested2.nested.si32_test, 42);
    EXPECT_EQ(test_obj.nested2.nested.str_test, "deeply_nested");
}

// Partial deserialization tests
TEST_F(WBEDeserializerYAMLTest, PartialDeserialization_MissingFields) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;

    // Initialize with known values
    test_obj.si32_test = 999;
    test_obj.si64_test = 888;
    test_obj.str_test = "original";
    test_obj.f32_test = 123.45F;

    WBE::ParserYAML parser;

    // YAML only contains some fields
    std::string partial_yaml = R"(si32_test: 42
str_test: "updated"
)";

    parser.parse_from_buffer(partial_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    // Updated fields
    EXPECT_EQ(test_obj.si32_test, 42);
    EXPECT_EQ(test_obj.str_test, "updated");

    // Unchanged fields (should retain original values)
    EXPECT_EQ(test_obj.si64_test, 888);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 123.45F);
}

TEST_F(WBEDeserializerYAMLTest, PartialDeserialization_ExtraFields) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // YAML contains extra fields that don't exist in the struct
    std::string extra_fields_yaml = R"(si32_test: 42
nonexistent_field: "should_be_ignored"
another_unknown: 999
str_test: "valid_field"
)";

    parser.parse_from_buffer(extra_fields_yaml);
    // Should not throw and should process valid fields
    EXPECT_NO_THROW(sd.deserialize(parser.get_data(), test_obj));

    EXPECT_EQ(test_obj.si32_test, 42);
    EXPECT_EQ(test_obj.str_test, "valid_field");
}

// GLM vector edge cases
TEST_F(WBEDeserializerYAMLTest, GLMVectors_EdgeCases) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    std::string vector_edge_yaml = R"(vec3_test:
  x: -0.0
  y: 1e-10
  z: 1e10
vec4_test:
  x: 0.000001
  y: -999999.999999
  z: 3.14159265359
  w: -3.14159265359
)";

    parser.parse_from_buffer(vector_edge_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_FLOAT_EQ(test_obj.vec3_test.x, -0.0F);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.y, 1e-10F);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.z, 1e10F);

    EXPECT_FLOAT_EQ(test_obj.vec4_test.x, 0.000001F);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.y, -999999.999999F);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.z, 3.14159265359F);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.w, -3.14159265359F);
}

TEST_F(WBEDeserializerYAMLTest, GLMVectors_CompleteSpecification) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test complete vector specification (all components required)
    std::string complete_vector_yaml = R"(vec3_test:
  x: 1.0
  y: 2.0
  z: 3.0
vec4_test:
  x: 1.0
  y: 2.0
  z: 3.0
  w: 4.0
)";

    parser.parse_from_buffer(complete_vector_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_FLOAT_EQ(test_obj.vec3_test.x, 1.0F);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.y, 2.0F);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.z, 3.0F);

    EXPECT_FLOAT_EQ(test_obj.vec4_test.x, 1.0F);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.y, 2.0F);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.z, 3.0F);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.w, 4.0F);
}

TEST_F(WBEDeserializerYAMLTest, GLMVectors_ZeroVectors) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test zero vectors (all components specified as zero)
    std::string zero_vector_yaml = R"(vec3_test:
  x: 0.0
  y: 0.0
  z: 0.0
vec4_test:
  x: 0.0
  y: 0.0
  z: 0.0
  w: 0.0
)";

    parser.parse_from_buffer(zero_vector_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.vec3_test, glm::vec3(0.0F, 0.0F, 0.0F));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(0.0F, 0.0F, 0.0F, 0.0F));
}

TEST_F(WBEDeserializerYAMLTest, GLMVectors_NegativeValues) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test negative vector values (all components specified)
    std::string negative_vector_yaml = R"(vec3_test:
  x: -1.5
  y: -2.5
  z: -3.5
vec4_test:
  x: -10.0
  y: -20.0
  z: -30.0
  w: -40.0
)";

    parser.parse_from_buffer(negative_vector_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.vec3_test, glm::vec3(-1.5F, -2.5F, -3.5F));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(-10.0F, -20.0F, -30.0F, -40.0F));
}

TEST_F(WBEDeserializerYAMLTest, GLMVectors_MissingComponentsError) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserYAML parser;

    // Test vec3 missing z component
    std::string missing_z_yaml = R"(vec3_test:
  x: 1.0
  y: 2.0
)";

    parser.parse_from_buffer(missing_z_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);

    // Test vec4 missing w component
    std::string missing_w_yaml = R"(vec4_test:
  x: 1.0
  y: 2.0
  z: 3.0
)";

    parser.parse_from_buffer(missing_w_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

// Tests for new required fields structures
TEST_F(WBEDeserializerYAMLTest, RequiredFields_BaseClass) {
    WBE::SerializableSD<WBE::TestRequiredFieldsBase> sd;
    WBE::TestRequiredFieldsBase test_obj;
    WBE::ParserYAML parser;

    std::string required_yaml = R"(required_id: 123
required_name: "test_base"
optional_value: 2.5
)";

    parser.parse_from_buffer(required_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestRequiredFieldsBase>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.required_id, 123);
    EXPECT_EQ(test_obj.required_name, "test_base");
    EXPECT_FLOAT_EQ(test_obj.optional_value, 2.5F);
}

TEST_F(WBEDeserializerYAMLTest, RequiredFields_BaseClass_MissingRequired) {
    WBE::SerializableSD<WBE::TestRequiredFieldsBase> sd;
    WBE::TestRequiredFieldsBase test_obj;
    WBE::ParserYAML parser;

    std::string missing_required_yaml = R"(required_id: 123
)";

    parser.parse_from_buffer(missing_required_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, RequiredFields_ChildInheritance) {
    WBE::SerializableSD<WBE::TestRequiredFieldsChild> sd;
    WBE::TestRequiredFieldsChild test_obj;
    WBE::ParserYAML parser;

    std::string child_required_yaml = R"(required_id: 456
required_name: "test_child"
optional_value: 3.14
required_child_field: "child_data"
optional_child_value: 5.5
required_vector:
  x: 1.0
  y: 2.0
  z: 3.0
)";

    parser.parse_from_buffer(child_required_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestRequiredFieldsChild>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.required_id, 456);
    EXPECT_EQ(test_obj.required_name, "test_child");
    EXPECT_FLOAT_EQ(test_obj.optional_value, 3.14F);
    EXPECT_EQ(test_obj.required_child_field, "child_data");
    EXPECT_DOUBLE_EQ(test_obj.optional_child_value, 5.5);
    EXPECT_EQ(test_obj.required_vector, glm::vec3(1.0F, 2.0F, 3.0F));
}

TEST_F(WBEDeserializerYAMLTest, RequiredFields_MultipleInheritance) {
    WBE::SerializableSD<WBE::TestMultipleRequiredChild> sd;
    WBE::TestMultipleRequiredChild test_obj;
    WBE::ParserYAML parser;

    std::string multiple_required_yaml = R"(required_a_id: 100
optional_a_name: "a_name"
required_b_value: 2.71
optional_b_desc: "b_description"
required_child_info: "child_info"
optional_child_count: 42
)";

    parser.parse_from_buffer(multiple_required_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestMultipleRequiredChild>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.required_a_id, 100);
    EXPECT_EQ(test_obj.optional_a_name, "a_name");
    EXPECT_FLOAT_EQ(test_obj.required_b_value, 2.71F);
    EXPECT_EQ(test_obj.optional_b_desc, "b_description");
    EXPECT_EQ(test_obj.required_child_info, "child_info");
    EXPECT_EQ(test_obj.optional_child_count, 42);
}

TEST_F(WBEDeserializerYAMLTest, RequiredFields_DiamondInheritance) {
    WBE::SerializableSD<WBE::TestDiamondRequiredChild> sd;
    WBE::TestDiamondRequiredChild test_obj;
    WBE::ParserYAML parser;

    std::string diamond_required_yaml = R"(required_diamond_base: "diamond_base_value"
optional_diamond_id: 999
required_left_data: 123.456
required_right_vector:
  x: 5.0
  y: 6.0
required_final_field: "final_value"
optional_final_flag: true
)";

    parser.parse_from_buffer(diamond_required_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestDiamondRequiredChild>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.required_diamond_base, "diamond_base_value");
    EXPECT_EQ(test_obj.optional_diamond_id, 999);
    EXPECT_DOUBLE_EQ(test_obj.required_left_data, 123.456);
    EXPECT_EQ(test_obj.required_right_vector, glm::vec2(5.0F, 6.0F));
    EXPECT_EQ(test_obj.required_final_field, "final_value");
    EXPECT_TRUE(test_obj.optional_final_flag);
}

// Tests for static serializable structures (WBE_SERIALIZABLE_STATIC)
TEST_F(WBEDeserializerYAMLTest, StaticSerializable_Basic) {
    WBE::SerializableSD<WBE::TestStaticSerializable> sd;
    WBE::TestStaticSerializable test_obj;
    WBE::ParserYAML parser;

    std::string static_yaml = R"(static_id: 789
static_name: "static_test"
static_value: 9.87
)";

    parser.parse_from_buffer(static_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestStaticSerializable>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.static_id, 789);
    EXPECT_EQ(test_obj.static_name, "static_test");
    EXPECT_FLOAT_EQ(test_obj.static_value, 9.87F);
}

TEST_F(WBEDeserializerYAMLTest, StaticSerializable_WithRequired) {
    WBE::SerializableSD<WBE::TestStaticWithRequired> sd;
    WBE::TestStaticWithRequired test_obj;
    WBE::ParserYAML parser;

    std::string static_required_yaml = R"(required_static_field: "static_required"
optional_static_number: 100
required_static_vector:
  x: 7.0
  y: 8.0
  z: 9.0
)";

    parser.parse_from_buffer(static_required_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestStaticWithRequired>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.required_static_field, "static_required");
    EXPECT_EQ(test_obj.optional_static_number, 100);
    EXPECT_EQ(test_obj.required_static_vector, glm::vec3(7.0F, 8.0F, 9.0F));
}

TEST_F(WBEDeserializerYAMLTest, StaticSerializable_WithRequired_MissingRequired) {
    WBE::SerializableSD<WBE::TestStaticWithRequired> sd;
    WBE::TestStaticWithRequired test_obj;
    WBE::ParserYAML parser;

    std::string missing_static_required_yaml = R"(optional_static_number: 100
)";

    parser.parse_from_buffer(missing_static_required_yaml);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerYAMLTest, StaticSerializable_Complex) {
    WBE::SerializableSD<WBE::TestStaticComplex> sd;
    WBE::TestStaticComplex test_obj;
    WBE::ParserYAML parser;

    std::string static_complex_yaml = R"(static_numbers:
  - 1
  - 2
  - 3
  - 4
  - 5
static_strings:
  - "static1"
  - "static2"
required_static_buffer: "static_buffer_data"
nested_static:
  static_id: 555
  static_name: "nested_static"
  static_value: 1.23
)";

    parser.parse_from_buffer(static_complex_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestStaticComplex>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.static_numbers.size(), 5);
    EXPECT_EQ(test_obj.static_numbers[0], 1);
    EXPECT_EQ(test_obj.static_numbers[4], 5);
    EXPECT_EQ(test_obj.static_strings.size(), 2);
    EXPECT_EQ(test_obj.static_strings[0], "static1");
    EXPECT_EQ(test_obj.static_strings[1], "static2");
    EXPECT_STREQ(test_obj.required_static_buffer.buffer, "static_buffer_data");
    EXPECT_EQ(test_obj.nested_static.static_id, 555);
    EXPECT_EQ(test_obj.nested_static.static_name, "nested_static");
    EXPECT_FLOAT_EQ(test_obj.nested_static.static_value, 1.23F);
}

TEST_F(WBEDeserializerYAMLTest, StaticSerializable_Empty) {
    WBE::SerializableSD<WBE::TestStaticEmpty> sd;
    WBE::TestStaticEmpty test_obj;
    WBE::ParserYAML parser;

    parser.parse_from_buffer("{}");
    EXPECT_NO_THROW(sd.deserialize(parser.get_data(), test_obj));
}

TEST_F(WBEDeserializerYAMLTest, StaticSerializable_VectorTypes) {
    WBE::SerializableSD<WBE::TestStaticVectorTypes> sd;
    WBE::TestStaticVectorTypes test_obj;
    WBE::ParserYAML parser;

    std::string static_vector_yaml = R"(vec2_field:
  x: 10.0
  y: 11.0
vec3_field:
  x: 12.0
  y: 13.0
  z: 14.0
vec4_field:
  x: 15.0
  y: 16.0
  z: 17.0
  w: 18.0
required_identifier: "vector_test"
)";

    parser.parse_from_buffer(static_vector_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestStaticVectorTypes>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.vec2_field, glm::vec2(10.0F, 11.0F));
    EXPECT_EQ(test_obj.vec3_field, glm::vec3(12.0F, 13.0F, 14.0F));
    EXPECT_EQ(test_obj.vec4_field, glm::vec4(15.0F, 16.0F, 17.0F, 18.0F));
    EXPECT_EQ(test_obj.required_identifier, "vector_test");
}

TEST_F(WBEDeserializerYAMLTest, MixedContainer_RequiredAndStatic) {
    WBE::SerializableSD<WBE::TestMixedContainer> sd;
    WBE::TestMixedContainer test_obj;
    WBE::ParserYAML parser;

    std::string mixed_yaml = R"(required_objects:
  - required_id: 1
    required_name: "obj1"
    optional_value: 1.1
  - required_id: 2
    required_name: "obj2"
static_objects:
  - static_id: 10
    static_name: "static1"
    static_value: 2.2
required_child:
  required_id: 999
  required_name: "child_test"
  required_child_field: "child_value"
  required_vector:
    x: 1.0
    y: 2.0
    z: 3.0
optional_static:
  static_numbers:
    - 100
    - 200
  required_static_buffer: "mixed_buffer"
)";

    parser.parse_from_buffer(mixed_yaml);
    WhiteBirdEngine::SerializableSD<WhiteBirdEngine::TestMixedContainer>::deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.required_objects.size(), 2);
    EXPECT_EQ(test_obj.required_objects[0].required_id, 1);
    EXPECT_EQ(test_obj.required_objects[0].required_name, "obj1");
    EXPECT_FLOAT_EQ(test_obj.required_objects[0].optional_value, 1.1F);

    EXPECT_EQ(test_obj.static_objects.size(), 1);
    EXPECT_EQ(test_obj.static_objects[0].static_id, 10);
    EXPECT_EQ(test_obj.static_objects[0].static_name, "static1");

    EXPECT_EQ(test_obj.required_child.required_id, 999);
    EXPECT_EQ(test_obj.required_child.required_name, "child_test");
    EXPECT_EQ(test_obj.required_child.required_child_field, "child_value");

    EXPECT_EQ(test_obj.optional_static.static_numbers.size(), 2);
    EXPECT_EQ(test_obj.optional_static.static_numbers[0], 100);
    EXPECT_STREQ(test_obj.optional_static.required_static_buffer.buffer, "mixed_buffer");
}

#endif
