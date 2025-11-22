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
#include <limits>
#include <stdexcept>
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
    WBE::SerializableSD<WBE::TestEmptySerializable> sd;
    WBE::TestEmptySerializable test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer("{}");
    sd.deserialize(parser.get_data(), test_obj);
}

TEST_F(WBEDeserializerJSONTest, General) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
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
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
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
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    parser.parse_from_buffer(test_serialize_json_general);
    sd.deserialize(parser.get_data(), test_obj);

    EXPECT_EQ(test_obj.str_test, std::string("Hello!"));
    EXPECT_STREQ(test_obj.buffer_test.buffer, "how are you?");
}

TEST_F(WBEDeserializerJSONTest, BufferTooLongThrows) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;

    // Create JSON where buffer_test is longer than Buffer<16>
    std::string long_buffer_json = R"({ "buffer_test": ")";
    long_buffer_json += std::string(32, 'A');
    long_buffer_json += R"(" })";

    parser.parse_from_buffer(long_buffer_json);
    ASSERT_THROW(sd.deserialize(parser.get_data(), test_obj), std::runtime_error);
}

TEST_F(WBEDeserializerJSONTest, PartialUpdateOnlyOneField) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
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
    WBE::SerializableSD<WBE::TestSerializableNesting> sd;
    WBE::TestSerializableNesting test_obj;
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

// Edge case tests
TEST_F(WBEDeserializerJSONTest, EdgeCase_MalformedJSON) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test malformed JSON
    EXPECT_THROW(parser.parse_from_buffer("{ malformed json }"), std::exception);
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_EmptyJSON) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Initialize with sentinel values to verify no changes
    test_obj.si32_test = 999;
    test_obj.str_test = "unchanged";
    
    parser.parse_from_buffer("{}");
    sd.deserialize(parser.get_data(), test_obj);
    
    // Values should remain unchanged when not present in JSON
    EXPECT_EQ(test_obj.si32_test, 999);
    EXPECT_EQ(test_obj.str_test, "unchanged");
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_NullValues_ShouldFail) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test JSON with null values for non-pointer types - should fail
    std::string null_json = R"({
        "si32_test": null,
        "str_test": null,
        "buffer_test": null
    })";
    
    parser.parse_from_buffer(null_json);
    // Should throw exception since non-pointer types cannot be null
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_NullValues_IndividualFields) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test null value for integer field
    std::string null_int_json = R"({ "si32_test": null })";
    parser.parse_from_buffer(null_int_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
    
    // Test null value for buffer field
    std::string null_buffer_json = R"({ "buffer_test": null })";
    parser.parse_from_buffer(null_buffer_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
    
    // Test null value for float field
    std::string null_float_json = R"({ "f32_test": null })";
    parser.parse_from_buffer(null_float_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
    
    // Test null value for vector field
    std::string null_vector_json = R"({ "vec3_test": null })";
    parser.parse_from_buffer(null_vector_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_TypeMismatch_ShouldFail) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test type mismatches - should fail
    std::string mismatch_json = R"({
        "si32_test": "not_a_number",
        "str_test": 12345
    })";
    
    parser.parse_from_buffer(mismatch_json);
    // Should throw exception for type mismatches
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_TypeMismatch_IndividualTypes) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test string where integer expected
    std::string int_mismatch_json = R"({ "si32_test": "not_an_integer" })";
    parser.parse_from_buffer(int_mismatch_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
    
    // Test integer where string expected
    std::string string_mismatch_json = R"({ "str_test": 12345 })";
    parser.parse_from_buffer(string_mismatch_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_ExtremeValues) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string extreme_json = R"({
        "si32_test": 2147483647,
        "si64_test": -9223372036854775808,
        "ui32_test": 4294967295,
        "ui64_test": 18446744073709551615,
        "f32_test": 3.4028235e+38,
        "f64_test": -1.7976931348623157e+308
    })";
    
    parser.parse_from_buffer(extreme_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.si32_test, std::numeric_limits<int32_t>::max());
    EXPECT_EQ(test_obj.si64_test, std::numeric_limits<int64_t>::min());
    EXPECT_EQ(test_obj.ui32_test, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(test_obj.ui64_test, std::numeric_limits<uint64_t>::max());
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_UnicodeStrings) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string unicode_json = R"({
        "str_test": "Hello 世界 🌍 ñáéíóú"
    })";
    
    parser.parse_from_buffer(unicode_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.str_test, "Hello 世界 🌍 ñáéíóú");
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_LongStringsAndBuffers) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test very long string
    std::string long_string(1000, 'A');
    std::string long_string_json = R"({ "str_test": ")" + long_string + R"(" })";
    
    parser.parse_from_buffer(long_string_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.str_test, long_string);
}

TEST_F(WBEDeserializerJSONTest, EdgeCase_EmptyStringAndBuffer) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string empty_json = R"({
        "str_test": "",
        "buffer_test": ""
    })";
    
    parser.parse_from_buffer(empty_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.str_test, "");
    EXPECT_STREQ(test_obj.buffer_test.buffer, "");
}

// Vector container tests
TEST_F(WBEDeserializerJSONTest, VectorContainer_EmptyVectors) {
    WBE::SerializableSD<WBE::TestVectorContainer> sd;
    WBE::TestVectorContainer test_obj;
    WBE::ParserJSON parser;
    
    std::string empty_vectors_json = R"({
        "ints": [],
        "strs": [],
        "children": []
    })";
    
    parser.parse_from_buffer(empty_vectors_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_TRUE(test_obj.ints.empty());
    EXPECT_TRUE(test_obj.strs.empty());
    EXPECT_TRUE(test_obj.children.empty());
}

TEST_F(WBEDeserializerJSONTest, VectorContainer_PopulatedVectors) {
    WBE::SerializableSD<WBE::TestVectorContainer> sd;
    WBE::TestVectorContainer test_obj;
    WBE::ParserJSON parser;
    
    std::string vectors_json = R"({
        "ints": [1, 2, 3, -4, 0],
        "strs": ["hello", "world", "", "test"],
        "children": [
            {
                "si32_test": 10,
                "str_test": "child1"
            },
            {
                "si32_test": 20,
                "str_test": "child2"
            }
        ]
    })";
    
    parser.parse_from_buffer(vectors_json);
    sd.deserialize(parser.get_data(), test_obj);
    
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

TEST_F(WBEDeserializerJSONTest, VectorContainer_LargeVector) {
    WBE::SerializableSD<WBE::TestVectorContainer> sd;
    WBE::TestVectorContainer test_obj;
    WBE::ParserJSON parser;
    
    // Create JSON with 1000 integers
    std::string large_vector_json = R"({ "ints": [)";
    for (int i = 0; i < 1000; ++i) {
        large_vector_json += std::to_string(i);
        if (i < 999) large_vector_json += ",";
    }
    large_vector_json += "]}";
    
    parser.parse_from_buffer(large_vector_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.ints.size(), 1000);
    EXPECT_EQ(test_obj.ints[0], 0);
    EXPECT_EQ(test_obj.ints[999], 999);
}

// Deep nesting tests
TEST_F(WBEDeserializerJSONTest, DeepNesting_Depth2) {
    WBE::SerializableSD<WBE::TestDepth2> sd;
    WBE::TestDepth2 test_obj;
    WBE::ParserJSON parser;

    std::string depth2_json = R"({
        "depth2_id": 100,
        "depth2_name": "depth2_test",
        "nested": {
            "si32_test": 42,
            "str_test": "nested_string",
            "vec3_test": {"x": 1.0, "y": 2.0, "z": 3.0}
        }
    })";
    
    parser.parse_from_buffer(depth2_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.depth2_id, 100);
    EXPECT_EQ(test_obj.depth2_name, "depth2_test");
    EXPECT_EQ(test_obj.nested.si32_test, 42);
    EXPECT_EQ(test_obj.nested.str_test, "nested_string");
    EXPECT_EQ(test_obj.nested.vec3_test, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(WBEDeserializerJSONTest, DeepNesting_Depth3) {
    WBE::SerializableSD<WBE::TestDepth3> sd;
    WBE::TestDepth3 test_obj;
    WBE::ParserJSON parser;
    
    std::string depth3_json = R"({
        "depth3_id": 200,
        "depth3_name": "depth3_test",
        "nested2": {
            "depth2_id": 100,
            "depth2_name": "depth2_nested",
            "nested": {
                "si32_test": 42,
                "str_test": "deeply_nested"
            }
        }
    })";
    
    parser.parse_from_buffer(depth3_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.depth3_id, 200);
    EXPECT_EQ(test_obj.depth3_name, "depth3_test");
    EXPECT_EQ(test_obj.nested2.depth2_id, 100);
    EXPECT_EQ(test_obj.nested2.depth2_name, "depth2_nested");
    EXPECT_EQ(test_obj.nested2.nested.si32_test, 42);
    EXPECT_EQ(test_obj.nested2.nested.str_test, "deeply_nested");
}

// Partial deserialization tests
TEST_F(WBEDeserializerJSONTest, PartialDeserialization_MissingFields) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    
    // Initialize with known values
    test_obj.si32_test = 999;
    test_obj.si64_test = 888;
    test_obj.str_test = "original";
    test_obj.f32_test = 123.45f;
    
    WBE::ParserJSON parser;
    
    // JSON only contains some fields
    std::string partial_json = R"({
        "si32_test": 42,
        "str_test": "updated"
    })";
    
    parser.parse_from_buffer(partial_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    // Updated fields
    EXPECT_EQ(test_obj.si32_test, 42);
    EXPECT_EQ(test_obj.str_test, "updated");
    
    // Unchanged fields (should retain original values)
    EXPECT_EQ(test_obj.si64_test, 888);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 123.45f);
}

TEST_F(WBEDeserializerJSONTest, PartialDeserialization_ExtraFields) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // JSON contains extra fields that don't exist in the struct
    std::string extra_fields_json = R"({
        "si32_test": 42,
        "nonexistent_field": "should_be_ignored",
        "another_unknown": 999,
        "str_test": "valid_field"
    })";
    
    parser.parse_from_buffer(extra_fields_json);
    // Should not throw and should process valid fields
    EXPECT_NO_THROW(sd.deserialize(parser.get_data(), test_obj));
    
    EXPECT_EQ(test_obj.si32_test, 42);
    EXPECT_EQ(test_obj.str_test, "valid_field");
}

// GLM vector edge cases
TEST_F(WBEDeserializerJSONTest, GLMVectors_EdgeCases) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string vector_edge_json = R"({
        "vec3_test": {"x": -0.0, "y": 1e-10, "z": 1e10},
        "vec4_test": {"x": 0.000001, "y": -999999.999999, "z": 3.14159265359, "w": -3.14159265359}
    })";
    
    parser.parse_from_buffer(vector_edge_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_FLOAT_EQ(test_obj.vec3_test.x, -0.0f);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.y, 1e-10f);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.z, 1e10f);
    
    EXPECT_FLOAT_EQ(test_obj.vec4_test.x, 0.000001f);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.y, -999999.999999f);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.z, 3.14159265359f);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.w, -3.14159265359f);
}

TEST_F(WBEDeserializerJSONTest, GLMVectors_CompleteSpecification) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test complete vector specification (all components required)
    std::string complete_vector_json = R"({
        "vec3_test": {"x": 1.0, "y": 2.0, "z": 3.0},
        "vec4_test": {"x": 1.0, "y": 2.0, "z": 3.0, "w": 4.0}
    })";
    
    parser.parse_from_buffer(complete_vector_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_FLOAT_EQ(test_obj.vec3_test.x, 1.0f);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.y, 2.0f);
    EXPECT_FLOAT_EQ(test_obj.vec3_test.z, 3.0f);
    
    EXPECT_FLOAT_EQ(test_obj.vec4_test.x, 1.0f);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.y, 2.0f);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.z, 3.0f);
    EXPECT_FLOAT_EQ(test_obj.vec4_test.w, 4.0f);
}

TEST_F(WBEDeserializerJSONTest, GLMVectors_ZeroVectors) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test zero vectors (all components specified as zero)
    std::string zero_vector_json = R"({
        "vec3_test": {"x": 0.0, "y": 0.0, "z": 0.0},
        "vec4_test": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 0.0}
    })";
    
    parser.parse_from_buffer(zero_vector_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
}

TEST_F(WBEDeserializerJSONTest, GLMVectors_NegativeValues) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test negative vector values (all components specified)
    std::string negative_vector_json = R"({
        "vec3_test": {"x": -1.5, "y": -2.5, "z": -3.5},
        "vec4_test": {"x": -10.0, "y": -20.0, "z": -30.0, "w": -40.0}
    })";
    
    parser.parse_from_buffer(negative_vector_json);
    sd.deserialize(parser.get_data(), test_obj);
    
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(-1.5f, -2.5f, -3.5f));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(-10.0f, -20.0f, -30.0f, -40.0f));
}

TEST_F(WBEDeserializerJSONTest, GLMVectors_MissingComponentsError) {
    WBE::SerializableSD<WBE::TestSerializable> sd;
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test vec3 missing z component
    std::string missing_z_json = R"({
        "vec3_test": {"x": 1.0, "y": 2.0}
    })";
    
    parser.parse_from_buffer(missing_z_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
    
    // Test vec4 missing w component
    std::string missing_w_json = R"({
        "vec4_test": {"x": 1.0, "y": 2.0, "z": 3.0}
    })";
    
    parser.parse_from_buffer(missing_w_json);
    EXPECT_THROW(sd.deserialize(parser.get_data(), test_obj), std::exception);
}

// Dynamic deserialization tests
TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_Basic) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    parser.parse_from_buffer(test_serialize_json_general);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.si32_test, 3);
    EXPECT_EQ(test_obj.si64_test, -62);
    EXPECT_EQ(test_obj.ui32_test, 42);
    EXPECT_EQ(test_obj.ui64_test, 59);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 3.14f);
    EXPECT_FLOAT_EQ(test_obj.f64_test, 2.718);
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(1, -2, 3));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(1, -2, 3, -4));
    EXPECT_EQ(test_obj.str_test, "Hello!");
    EXPECT_STREQ(test_obj.buffer_test.buffer, "how are you?");
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_EmptySerializable) {
    WBE::TestEmptySerializable test_obj;
    WBE::ParserJSON parser;
    
    parser.parse_from_buffer("{}");
    EXPECT_NO_THROW(test_obj.deserialize(parser.get_data()));
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_Nesting) {
    WBE::TestSerializableNesting test_obj;
    WBE::ParserJSON parser;
    
    parser.parse_from_buffer(test_serialize_json_nesting);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.nesting_id, 7);
    EXPECT_EQ(test_obj.name, "parent");
    EXPECT_STREQ(test_obj.small_buffer.buffer, "abc");
    
    // Check nested object was properly deserialized
    EXPECT_EQ(test_obj.nested_test.si32_test, 3);
    EXPECT_EQ(test_obj.nested_test.str_test, "Hello!");
    EXPECT_FLOAT_EQ(test_obj.nested_test.f32_test, 3.14f);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_VectorContainer) {
    WBE::TestVectorContainer test_obj;
    WBE::ParserJSON parser;
    
    std::string vectors_json = R"({
        "ints": [1, 2, 3, -4, 0],
        "strs": ["hello", "world", "", "test"],
        "children": [
            {
                "si32_test": 10,
                "str_test": "child1"
            },
            {
                "si32_test": 20,
                "str_test": "child2"
            }
        ]
    })";
    
    parser.parse_from_buffer(vectors_json);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.ints.size(), 5);
    EXPECT_EQ(test_obj.ints[0], 1);
    EXPECT_EQ(test_obj.ints[3], -4);
    
    EXPECT_EQ(test_obj.strs.size(), 4);
    EXPECT_EQ(test_obj.strs[0], "hello");
    EXPECT_EQ(test_obj.strs[2], "");
    
    EXPECT_EQ(test_obj.children.size(), 2);
    EXPECT_EQ(test_obj.children[0].si32_test, 10);
    EXPECT_EQ(test_obj.children[0].str_test, "child1");
    EXPECT_EQ(test_obj.children[1].si32_test, 20);
    EXPECT_EQ(test_obj.children[1].str_test, "child2");
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_DeepNesting) {
    WBE::TestDepth3 test_obj;
    WBE::ParserJSON parser;
    
    std::string depth3_json = R"({
        "depth3_id": 200,
        "depth3_name": "depth3_test",
        "nested2": {
            "depth2_id": 100,
            "depth2_name": "depth2_nested",
            "nested": {
                "si32_test": 42,
                "str_test": "deeply_nested",
                "vec3_test": {"x": 1.0, "y": 2.0, "z": 3.0}
            }
        }
    })";
    
    parser.parse_from_buffer(depth3_json);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.depth3_id, 200);
    EXPECT_EQ(test_obj.depth3_name, "depth3_test");
    EXPECT_EQ(test_obj.nested2.depth2_id, 100);
    EXPECT_EQ(test_obj.nested2.depth2_name, "depth2_nested");
    EXPECT_EQ(test_obj.nested2.nested.si32_test, 42);
    EXPECT_EQ(test_obj.nested2.nested.str_test, "deeply_nested");
    EXPECT_EQ(test_obj.nested2.nested.vec3_test, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_PartialUpdate) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Initialize with known values
    test_obj.si32_test = 999;
    test_obj.si64_test = 888;
    test_obj.str_test = "original";
    test_obj.f32_test = 123.45f;
    
    // JSON only contains some fields
    std::string partial_json = R"({
        "si32_test": 42,
        "str_test": "updated"
    })";
    
    parser.parse_from_buffer(partial_json);
    test_obj.deserialize(parser.get_data());
    
    // Updated fields
    EXPECT_EQ(test_obj.si32_test, 42);
    EXPECT_EQ(test_obj.str_test, "updated");
    
    // Unchanged fields (should retain original values)
    EXPECT_EQ(test_obj.si64_test, 888);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 123.45f);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ErrorHandling_NullValues) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string null_json = R"({
        "si32_test": null,
        "str_test": null
    })";
    
    parser.parse_from_buffer(null_json);
    // Should throw exception for null values on non-pointer types
    EXPECT_THROW(test_obj.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ErrorHandling_TypeMismatch) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string mismatch_json = R"({
        "si32_test": "not_a_number",
        "str_test": 12345
    })";
    
    parser.parse_from_buffer(mismatch_json);
    // Should throw exception for type mismatches
    EXPECT_THROW(test_obj.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ErrorHandling_MalformedJSON) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Test malformed JSON
    EXPECT_THROW(parser.parse_from_buffer("{ malformed json }"), std::exception);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ErrorHandling_BufferOverflow) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    // Create JSON where buffer_test is longer than Buffer<16>
    std::string long_buffer_json = R"({ "buffer_test": ")";
    long_buffer_json += std::string(32, 'A');
    long_buffer_json += R"(" })";
    
    parser.parse_from_buffer(long_buffer_json);
    // Should throw exception for buffer overflow
    EXPECT_THROW(test_obj.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ErrorHandling_IncompleteVectors) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string incomplete_vector_json = R"({
        "vec3_test": {"x": 1.0, "y": 2.0}
    })";
    
    parser.parse_from_buffer(incomplete_vector_json);
    // Should throw exception for incomplete vector components
    EXPECT_THROW(test_obj.deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ExtremeValues) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string extreme_json = R"({
        "si32_test": 2147483647,
        "si64_test": -9223372036854775808,
        "ui32_test": 4294967295,
        "ui64_test": 18446744073709551615,
        "f32_test": 3.4028235e+38,
        "f64_test": -1.7976931348623157e+308
    })";
    
    parser.parse_from_buffer(extreme_json);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.si32_test, std::numeric_limits<int32_t>::max());
    EXPECT_EQ(test_obj.si64_test, std::numeric_limits<int64_t>::min());
    EXPECT_EQ(test_obj.ui32_test, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(test_obj.ui64_test, std::numeric_limits<uint64_t>::max());
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_UnicodeStrings) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    std::string unicode_json = R"({
        "str_test": "Hello 世界 🌍 ñáéíóú"
    })";
    
    parser.parse_from_buffer(unicode_json);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.str_test, "Hello 世界 🌍 ñáéíóú");
}

TEST_F(WBEDeserializerJSONTest, DynamicDeserialization_ZeroValues) {
    WBE::TestSerializable test_obj;
    WBE::ParserJSON parser;
    
    parser.parse_from_buffer(test_serialize_json_zeros);
    test_obj.deserialize(parser.get_data());
    
    EXPECT_EQ(test_obj.si32_test, 0);
    EXPECT_EQ(test_obj.si64_test, 0);
    EXPECT_EQ(test_obj.ui32_test, 0u);
    EXPECT_EQ(test_obj.ui64_test, 0u);
    EXPECT_FLOAT_EQ(test_obj.f32_test, 0.0f);
    EXPECT_FLOAT_EQ(test_obj.f64_test, 0.0);
    EXPECT_EQ(test_obj.vec3_test, glm::vec3(0,0,0));
    EXPECT_EQ(test_obj.vec4_test, glm::vec4(0,0,0,0));
    EXPECT_EQ(test_obj.str_test, "");
    EXPECT_STREQ(test_obj.buffer_test.buffer, "");
}

// Dynamic dispatch tests using WBE::Serializable base class
TEST_F(WBEDeserializerJSONTest, DynamicDispatch_SingleInheritance_BasePointer) {
    std::unique_ptr<WBE::Serializable> test_obj = std::make_unique<WBE::TestInheritedChild>();
    WBE::ParserJSON parser;
    
    std::string inheritance_json = R"({
        "base_id": 100,
        "base_name": "base_object",
        "base_value": 3.14,
        "child_id": 200,
        "child_name": "child_object",
        "child_value": 2.718
    })";
    
    parser.parse_from_buffer(inheritance_json);
    // Dynamic dispatch - should call TestInheritedChild::deserialize
    test_obj->deserialize(parser.get_data());
    
    // Cast back to verify proper deserialization
    WBE::TestInheritedChild* child_ptr = dynamic_cast<WBE::TestInheritedChild*>(test_obj.get());
    ASSERT_NE(child_ptr, nullptr);
    
    // Check base class fields
    EXPECT_EQ(child_ptr->base_id, 100);
    EXPECT_EQ(child_ptr->base_name, "base_object");
    EXPECT_FLOAT_EQ(child_ptr->base_value, 3.14f);
    
    // Check child class fields
    EXPECT_EQ(child_ptr->child_id, 200);
    EXPECT_EQ(child_ptr->child_name, "child_object");
    EXPECT_DOUBLE_EQ(child_ptr->child_value, 2.718);
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_MultilevelInheritance_BasePointer) {
    std::unique_ptr<WBE::Serializable> test_obj = std::make_unique<WBE::TestInheritedGrandchild>();
    WBE::ParserJSON parser;
    
    std::string grandchild_json = R"({
        "base_id": 100,
        "base_name": "base_level",
        "base_value": 1.0,
        "child_id": 200,
        "child_name": "child_level",
        "child_value": 2.0,
        "grandchild_id": 300,
        "grandchild_name": "grandchild_level",
        "grandchild_vector": {"x": 1.0, "y": 2.0, "z": 3.0}
    })";
    
    parser.parse_from_buffer(grandchild_json);
    // Dynamic dispatch - should call TestInheritedGrandchild::deserialize
    test_obj->deserialize(parser.get_data());
    
    // Cast back to verify proper deserialization
    WBE::TestInheritedGrandchild* grandchild_ptr = dynamic_cast<WBE::TestInheritedGrandchild*>(test_obj.get());
    ASSERT_NE(grandchild_ptr, nullptr);
    
    // Check all levels of inheritance
    EXPECT_EQ(grandchild_ptr->base_id, 100);
    EXPECT_EQ(grandchild_ptr->base_name, "base_level");
    EXPECT_FLOAT_EQ(grandchild_ptr->base_value, 1.0f);
    EXPECT_EQ(grandchild_ptr->child_id, 200);
    EXPECT_EQ(grandchild_ptr->child_name, "child_level");
    EXPECT_DOUBLE_EQ(grandchild_ptr->child_value, 2.0);
    EXPECT_EQ(grandchild_ptr->grandchild_id, 300);
    EXPECT_EQ(grandchild_ptr->grandchild_name, "grandchild_level");
    EXPECT_EQ(grandchild_ptr->grandchild_vector, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_MultipleInheritance_BasePointer) {
    std::unique_ptr<WBE::TestMultipleInheritanceA> test_obj_a = std::make_unique<WBE::TestMultipleInheritanceChild>();
    std::unique_ptr<WBE::TestMultipleInheritanceB> test_obj_b = std::make_unique<WBE::TestMultipleInheritanceChild>();
    
    std::string multiple_inheritance_json = R"({
        "a_id": 100,
        "a_name": "from_a",
        "a_value": 1.5,
        "b_id": 200,
        "b_name": "from_b",
        "b_value": 2.5,
        "child_id": 300,
        "child_name": "multiple_child",
        "child_vector": {"x": 10.0, "y": 20.0}
    })";

    auto test_func = [&multiple_inheritance_json](WBE::Serializable* test_obj) {
        WBE::ParserJSON parser;
        parser.parse_from_buffer(multiple_inheritance_json);
        // Dynamic dispatch - should call TestMultipleInheritanceChild::deserialize
        test_obj->deserialize(parser.get_data());

        // Cast back to verify proper deserialization
        WBE::TestMultipleInheritanceChild* child_ptr = dynamic_cast<WBE::TestMultipleInheritanceChild*>(test_obj);
        ASSERT_NE(child_ptr, nullptr);

        // Check fields from both base classes and child
        EXPECT_EQ(child_ptr->a_id, 100);
        EXPECT_EQ(child_ptr->a_name, "from_a");
        EXPECT_FLOAT_EQ(child_ptr->a_value, 1.5f);
        EXPECT_EQ(child_ptr->b_id, 200);
        EXPECT_EQ(child_ptr->b_name, "from_b");
        EXPECT_DOUBLE_EQ(child_ptr->b_value, 2.5);
        EXPECT_EQ(child_ptr->child_id, 300);
        EXPECT_EQ(child_ptr->child_name, "multiple_child");
        EXPECT_EQ(child_ptr->child_vector, glm::vec2(10.0f, 20.0f));
    };
    test_func(test_obj_a.get());
    test_func(test_obj_b.get());
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_DiamondInheritance_BasePointer) {
    std::unique_ptr<WBE::Serializable> test_obj = std::make_unique<WBE::TestDiamondChild>();
    WBE::ParserJSON parser;
    
    std::string diamond_json = R"({
        "diamond_base_id": 50,
        "diamond_base_name": "diamond_base",
        "left_id": 100,
        "left_value": 3.14,
        "right_id": 200,
        "right_value": 2.718,
        "diamond_child_id": 300,
        "diamond_child_name": "diamond_child"
    })";
    
    parser.parse_from_buffer(diamond_json);
    // Dynamic dispatch - should call TestDiamondChild::deserialize
    test_obj->deserialize(parser.get_data());
    
    // Cast back to verify proper deserialization
    WBE::TestDiamondChild* diamond_ptr = dynamic_cast<WBE::TestDiamondChild*>(test_obj.get());
    ASSERT_NE(diamond_ptr, nullptr);
    
    // Check diamond pattern fields
    EXPECT_EQ(diamond_ptr->diamond_base_id, 50);
    EXPECT_EQ(diamond_ptr->diamond_base_name, "diamond_base");
    EXPECT_EQ(diamond_ptr->left_id, 100);
    EXPECT_FLOAT_EQ(diamond_ptr->left_value, 3.14f);
    EXPECT_EQ(diamond_ptr->right_id, 200);
    EXPECT_DOUBLE_EQ(diamond_ptr->right_value, 2.718);
    EXPECT_EQ(diamond_ptr->diamond_child_id, 300);
    EXPECT_EQ(diamond_ptr->diamond_child_name, "diamond_child");
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_PolymorphicContainer) {
    std::vector<std::unique_ptr<WBE::Serializable>> objects;
    WBE::ParserJSON parser;
    
    // Create different types of objects
    objects.push_back(std::make_unique<WBE::TestSerializable>());
    objects.push_back(std::make_unique<WBE::TestInheritedChild>());
    objects.push_back(std::make_unique<WBE::TestInheritedGrandchild>());
    objects.push_back(std::make_unique<WBE::TestVectorContainer>());
    
    // Test basic serializable
    std::string basic_json = R"({
        "si32_test": 42,
        "str_test": "basic_test"
    })";
    parser.parse_from_buffer(basic_json);
    objects[0]->deserialize(parser.get_data());
    
    WBE::TestSerializable* basic_ptr = dynamic_cast<WBE::TestSerializable*>(objects[0].get());
    ASSERT_NE(basic_ptr, nullptr);
    EXPECT_EQ(basic_ptr->si32_test, 42);
    EXPECT_EQ(basic_ptr->str_test, "basic_test");
    
    // Test inherited child
    std::string child_json = R"({
        "base_id": 100,
        "base_name": "polymorphic_base",
        "child_id": 200,
        "child_name": "polymorphic_child"
    })";
    parser.parse_from_buffer(child_json);
    objects[1]->deserialize(parser.get_data());
    
    WBE::TestInheritedChild* child_ptr = dynamic_cast<WBE::TestInheritedChild*>(objects[1].get());
    ASSERT_NE(child_ptr, nullptr);
    EXPECT_EQ(child_ptr->base_id, 100);
    EXPECT_EQ(child_ptr->base_name, "polymorphic_base");
    EXPECT_EQ(child_ptr->child_id, 200);
    EXPECT_EQ(child_ptr->child_name, "polymorphic_child");
    
    // Test grandchild
    std::string grandchild_json = R"({
        "grandchild_id": 300,
        "grandchild_name": "polymorphic_grandchild"
    })";
    parser.parse_from_buffer(grandchild_json);
    objects[2]->deserialize(parser.get_data());
    
    WBE::TestInheritedGrandchild* grandchild_ptr = dynamic_cast<WBE::TestInheritedGrandchild*>(objects[2].get());
    ASSERT_NE(grandchild_ptr, nullptr);
    EXPECT_EQ(grandchild_ptr->grandchild_id, 300);
    EXPECT_EQ(grandchild_ptr->grandchild_name, "polymorphic_grandchild");
    
    // Test vector container
    std::string vector_json = R"({
        "ints": [1, 2, 3],
        "strs": ["poly", "morphic"]
    })";
    parser.parse_from_buffer(vector_json);
    objects[3]->deserialize(parser.get_data());
    
    WBE::TestVectorContainer* vector_ptr = dynamic_cast<WBE::TestVectorContainer*>(objects[3].get());
    ASSERT_NE(vector_ptr, nullptr);
    EXPECT_EQ(vector_ptr->ints.size(), 3);
    EXPECT_EQ(vector_ptr->ints[0], 1);
    EXPECT_EQ(vector_ptr->strs.size(), 2);
    EXPECT_EQ(vector_ptr->strs[0], "poly");
    EXPECT_EQ(vector_ptr->strs[1], "morphic");
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_RuntimeTypeResolution) {
    // Simulate runtime type resolution scenario
    auto create_object = [](const std::string& type_name) -> std::unique_ptr<WBE::Serializable> {
        if (type_name == "TestSerializable") {
            return std::make_unique<WBE::TestSerializable>();
        } else if (type_name == "TestInheritedChild") {
            return std::make_unique<WBE::TestInheritedChild>();
        } else if (type_name == "TestInheritedGrandchild") {
            return std::make_unique<WBE::TestInheritedGrandchild>();
        } else if (type_name == "TestVectorContainer") {
            return std::make_unique<WBE::TestVectorContainer>();
        }
        return nullptr;
    };
    
    struct TestCase {
        std::string type_name;
        std::string json_data;
        std::function<void(WBE::Serializable*)> validator;
    };
    
    std::vector<TestCase> test_cases = {
        {
            "TestSerializable",
            R"({"si32_test": 123, "str_test": "runtime_test"})",
            [](WBE::Serializable* obj) {
                auto* ptr = dynamic_cast<WBE::TestSerializable*>(obj);
                ASSERT_NE(ptr, nullptr);
                EXPECT_EQ(ptr->si32_test, 123);
                EXPECT_EQ(ptr->str_test, "runtime_test");
            }
        },
        {
            "TestInheritedChild",
            R"({"base_id": 456, "child_id": 789})",
            [](WBE::Serializable* obj) {
                auto* ptr = dynamic_cast<WBE::TestInheritedChild*>(obj);
                ASSERT_NE(ptr, nullptr);
                EXPECT_EQ(ptr->base_id, 456);
                EXPECT_EQ(ptr->child_id, 789);
            }
        },
        {
            "TestVectorContainer",
            R"({"ints": [10, 20, 30]})",
            [](WBE::Serializable* obj) {
                auto* ptr = dynamic_cast<WBE::TestVectorContainer*>(obj);
                ASSERT_NE(ptr, nullptr);
                EXPECT_EQ(ptr->ints.size(), 3);
                EXPECT_EQ(ptr->ints[1], 20);
            }
        }
    };
    
    WBE::ParserJSON parser;
    
    for (const auto& test_case : test_cases) {
        auto obj = create_object(test_case.type_name);
        ASSERT_NE(obj, nullptr) << "Failed to create object of type: " << test_case.type_name;
        
        parser.parse_from_buffer(test_case.json_data);
        // Dynamic dispatch based on runtime type
        obj->deserialize(parser.get_data());
        
        // Validate using type-specific validator
        test_case.validator(obj.get());
    }
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_ErrorHandling_BasePointer) {
    std::unique_ptr<WBE::Serializable> test_obj = std::make_unique<WBE::TestInheritedChild>();
    WBE::ParserJSON parser;
    
    // Test error handling with dynamic dispatch
    std::string error_json = R"({
        "base_id": null,
        "child_name": "test"
    })";
    
    parser.parse_from_buffer(error_json);
    // Should throw exception even with dynamic dispatch
    EXPECT_THROW(test_obj->deserialize(parser.get_data()), std::exception);
}

TEST_F(WBEDeserializerJSONTest, DynamicDispatch_PartialUpdate_BasePointer) {
    std::unique_ptr<WBE::Serializable> test_obj = std::make_unique<WBE::TestInheritedChild>();
    WBE::ParserJSON parser;
    
    // Initialize with known values through direct access
    WBE::TestInheritedChild* child_ptr = dynamic_cast<WBE::TestInheritedChild*>(test_obj.get());
    child_ptr->base_id = 999;
    child_ptr->base_name = "original";
    child_ptr->child_id = 888;
    child_ptr->child_name = "original_child";
    
    // Partial update through base pointer
    std::string partial_json = R"({
        "base_id": 111,
        "child_name": "updated_child"
    })";
    
    parser.parse_from_buffer(partial_json);
    test_obj->deserialize(parser.get_data());
    
    // Verify partial update worked correctly
    EXPECT_EQ(child_ptr->base_id, 111);
    EXPECT_EQ(child_ptr->child_name, "updated_child");
    EXPECT_EQ(child_ptr->base_name, "original");
    EXPECT_EQ(child_ptr->child_id, 888);
}

#endif
