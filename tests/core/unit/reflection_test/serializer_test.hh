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
#ifndef __WBE_SERIALIZER_TEST_HH__
#define __WBE_SERIALIZER_TEST_HH__

#include "core/parser/parser_json.hh"
#include "core/parser/parser_yaml.hh"
#include "global/global.hh"
#include "reflection_test_data.hh"
#include "generated/serializables_sd.gen.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace WBE = WhiteBirdEngine;

class WBESerializerTest : public ::testing::Test {
protected:

    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }
 
    std::unique_ptr<WBE::Global> global;
};
template <typename ParserDataType>
inline void serializer_test_default_construct(ParserDataType p_data) {
    TestSerializable serializable{};
    WBE::SerializableSD<TestSerializable> sd;
    sd.serialize(p_data, serializable);
    EXPECT_TRUE(p_data.contains("si32_test"));
    EXPECT_TRUE(p_data.contains("si64_test"));
    EXPECT_TRUE(p_data.contains("ui32_test"));
    EXPECT_TRUE(p_data.contains("ui64_test"));
    EXPECT_TRUE(p_data.contains("f32_test"));
    EXPECT_TRUE(p_data.contains("f64_test"));
    EXPECT_TRUE(p_data.contains("vec3_test"));
    EXPECT_TRUE(p_data.contains("vec4_test"));
    EXPECT_TRUE(p_data.contains("str_test"));
    EXPECT_TRUE(p_data.contains("buffer_test"));
    EXPECT_EQ(p_data.template get_value<int32_t>("si32_test"), 0);
    EXPECT_EQ(p_data.template get_value<int64_t>("si64_test"), 0);
    EXPECT_EQ(p_data.template get_value<uint32_t>("ui32_test"), 0);
    EXPECT_EQ(p_data.template get_value<uint64_t>("ui64_test"), 0);
    EXPECT_FLOAT_EQ(p_data.template get_value<float>("f32_test"), 0.0f);
    EXPECT_DOUBLE_EQ(p_data.template get_value<double>("f64_test"), 0.0);
    EXPECT_EQ(p_data.template get_value<glm::vec3>("vec3_test"), glm::vec3());
    EXPECT_EQ(p_data.template get_value<glm::vec4>("vec4_test"), glm::vec4());
    EXPECT_EQ(p_data.template get_value<std::string>("str_test"), std::string());
}

template <typename ParserDataType>
inline void serializer_test_general(ParserDataType p_data) {
    TestSerializable serializable;
    // populate with non-default values
    serializable.si32_test = 3;
    serializable.si64_test = -62;
    serializable.ui32_test = 42;
    serializable.ui64_test = 59;
    serializable.f32_test = 3.14f;
    serializable.f64_test = 2.718;
    serializable.vec3_test = glm::vec3(1, -2, 3);
    serializable.vec4_test = glm::vec4(1, -2, 3, -4);
    serializable.str_test = "Hello!";
    strcpy(serializable.buffer_test.buffer, "how are you?");

    WBE::SerializableSD<TestSerializable> sd;
    sd.serialize(p_data, serializable);

    // verify keys and values
    EXPECT_TRUE(p_data.contains("si32_test"));
    EXPECT_TRUE(p_data.contains("si64_test"));
    EXPECT_TRUE(p_data.contains("ui32_test"));
    EXPECT_TRUE(p_data.contains("ui64_test"));
    EXPECT_TRUE(p_data.contains("f32_test"));
    EXPECT_TRUE(p_data.contains("f64_test"));
    EXPECT_TRUE(p_data.contains("vec3_test"));
    EXPECT_TRUE(p_data.contains("vec4_test"));
    EXPECT_TRUE(p_data.contains("str_test"));
    EXPECT_TRUE(p_data.contains("buffer_test"));

    EXPECT_EQ(p_data.template get_value<int32_t>("si32_test"), 3);
    EXPECT_EQ(p_data.template get_value<int64_t>("si64_test"), -62);
    EXPECT_EQ(p_data.template get_value<uint32_t>("ui32_test"), 42u);
    EXPECT_EQ(p_data.template get_value<uint64_t>("ui64_test"), 59u);
    EXPECT_FLOAT_EQ(p_data.template get_value<float>("f32_test"), 3.14f);
    EXPECT_DOUBLE_EQ(p_data.template get_value<double>("f64_test"), 2.718);
    EXPECT_EQ(p_data.template get_value<glm::vec3>("vec3_test"), glm::vec3(1, -2, 3));
    EXPECT_EQ(p_data.template get_value<glm::vec4>("vec4_test"), glm::vec4(1, -2, 3, -4));
    EXPECT_EQ(p_data.template get_value<std::string>("str_test"), std::string("Hello!"));
    EXPECT_EQ(p_data.template get_value<std::string>("buffer_test"), std::string("how are you?"));
}

template <typename ParserDataType>
inline void serializer_test_nesting(ParserDataType p_data) {
    TestSerializableNesting serializable{};
    // fill nested_test
    serializable.nesting_id = 7;
    serializable.name = "parent";
    strcpy(serializable.small_buffer.buffer, "abc");
    serializable.nested_test.si32_test = 3;
    serializable.nested_test.si64_test = -62;
    serializable.nested_test.ui32_test = 42;
    serializable.nested_test.ui64_test = 59;
    serializable.nested_test.f32_test = 3.14f;
    serializable.nested_test.f64_test = 2.718;
    serializable.nested_test.vec3_test = glm::vec3(1, -2, 3);
    serializable.nested_test.vec4_test = glm::vec4(1, -2, 3, -4);
    serializable.nested_test.str_test = "Hello!";
    strcpy(serializable.nested_test.buffer_test.buffer, "how are you?");

    WBE::SerializableSD<TestSerializableNesting> sd;
    sd.serialize(p_data, serializable);

    // Top-level checks
    EXPECT_TRUE(p_data.contains("nesting_id"));
    EXPECT_TRUE(p_data.contains("name"));
    EXPECT_TRUE(p_data.contains("small_buffer"));
    EXPECT_TRUE(p_data.contains("nested_test"));

    EXPECT_EQ(p_data.template get_value<int32_t>("nesting_id"), 7);
    EXPECT_EQ(p_data.template get_value<std::string>("name"), std::string("parent"));
    EXPECT_EQ(p_data.template get_value<std::string>("small_buffer"), std::string("abc"));

    // Nested checks
    auto nested = p_data.template get_value<ParserDataType>("nested_test");
    EXPECT_EQ(nested.template get_value<int32_t>("si32_test"), 3);
    EXPECT_EQ(nested.template get_value<int64_t>("si64_test"), -62);
    EXPECT_EQ(nested.template get_value<uint32_t>("ui32_test"), 42u);
    EXPECT_EQ(nested.template get_value<uint64_t>("ui64_test"), 59u);
    EXPECT_FLOAT_EQ(nested.template get_value<float>("f32_test"), 3.14f);
    EXPECT_DOUBLE_EQ(nested.template get_value<double>("f64_test"), 2.718);
    EXPECT_EQ(nested.template get_value<glm::vec3>("vec3_test"), glm::vec3(1, -2, 3));
    EXPECT_EQ(nested.template get_value<glm::vec4>("vec4_test"), glm::vec4(1, -2, 3, -4));
    EXPECT_EQ(nested.template get_value<std::string>("str_test"), std::string("Hello!"));
    EXPECT_EQ(nested.template get_value<std::string>("buffer_test"), std::string("how are you?"));
}

template <typename ParserDataType>
inline void serializer_test_deep_nesting(ParserDataType p_data) {
    // Depth-2: TestDepth2 contains TestSerializable
    TestDepth2 td2{};
    td2.depth2_id = 123;
    td2.depth2_name = "depth2";
    td2.nested.si32_test = 9;
    td2.nested.str_test = "deep";
    strcpy(td2.nested.buffer_test.buffer, "B2");

    WBE::SerializableSD<TestDepth2> sd;
    sd.serialize(p_data, td2);

    EXPECT_TRUE(p_data.contains("nested"));
    EXPECT_TRUE(p_data.contains("depth2_id"));
    EXPECT_TRUE(p_data.contains("depth2_name"));

    auto nested = p_data.template get_value<ParserDataType>("nested");
    EXPECT_EQ(nested.template get_value<int32_t>("si32_test"), 9);
    EXPECT_EQ(nested.template get_value<std::string>("str_test"), std::string("deep"));
    EXPECT_EQ(nested.template get_value<std::string>("buffer_test"), std::string("B2"));
}

template <typename ParserDataType>
inline void serializer_test_vector_primitives(ParserDataType p_data_ints, ParserDataType p_data_strs) {
    // Test serializing vectors of primitives
    std::vector<int> ints = {1, 2, 3, 4, 5};
    WBE::SerializableSD<std::vector<int>> sd_ints;
    sd_ints.serialize(p_data_ints, ints);

    auto out_ints = p_data_ints.template get<std::vector<int>>();
    ASSERT_EQ(out_ints.size(), ints.size());
    for (size_t i = 0; i < ints.size(); ++i) EXPECT_EQ(out_ints[i], ints[i]);

    // Test vector<string>
    std::vector<std::string> strs = {"one", "two", "three"};
    WBE::SerializableSD<std::vector<std::string>> sd_strs;
    sd_strs.serialize(p_data_strs, strs);

    auto out_strs = p_data_strs.template get<std::vector<std::string>>();
    ASSERT_EQ(out_strs.size(), strs.size());
    for (size_t i = 0; i < strs.size(); ++i) EXPECT_EQ(out_strs[i], strs[i]);
}

template <typename ParserDataType>
inline void serializer_test_vector_serializables(ParserDataType p_data) {
    // Test serializing vector of TestSerializable objects
    std::vector<TestSerializable> vec;
    TestSerializable a{}; a.si32_test = 1; a.str_test = "a"; strcpy(a.buffer_test.buffer, "A");
    TestSerializable b{}; b.si32_test = 2; b.str_test = "b"; strcpy(b.buffer_test.buffer, "B");
    vec.push_back(a);
    vec.push_back(b);

    WBE::SerializableSD<std::vector<TestSerializable>> sd_vec;
    sd_vec.serialize(p_data, vec);

    // Expect an array of objects
    auto arr = p_data.template get<std::vector<ParserDataType>>();
    ASSERT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0].template get_value<int32_t>("si32_test"), 1);
    EXPECT_EQ(arr[0].template get_value<std::string>("str_test"), std::string("a"));
    EXPECT_EQ(arr[1].template get_value<int32_t>("si32_test"), 2);
    EXPECT_EQ(arr[1].template get_value<std::string>("str_test"), std::string("b"));
}

template <typename ParserDataType>
inline void serializer_test_struct_with_vector_serializables(ParserDataType p_data) {
    // Build a container with vectors
    TestVectorContainer container{};
    container.ints = {10, 20, 30};
    container.strs = {"aa", "bb"};

    TestSerializable a{}; a.si32_test = 5; a.str_test = "aa"; strcpy(a.buffer_test.buffer, "A");
    TestSerializable b{}; b.si32_test = 6; b.str_test = "bb"; strcpy(b.buffer_test.buffer, "B");
    container.children.push_back(a);
    container.children.push_back(b);

    WBE::SerializableSD<TestVectorContainer> sd;
    sd.serialize(p_data, container);

    // Top-level fields
    EXPECT_TRUE(p_data.contains("ints"));
    EXPECT_TRUE(p_data.contains("strs"));
    EXPECT_TRUE(p_data.contains("children"));

    // Inspect ints
    auto ints_out = p_data.template get_value<ParserDataType>("ints").template get<std::vector<int>>();
    ASSERT_EQ(ints_out.size(), 3u);
    EXPECT_EQ(ints_out[0], 10);
    EXPECT_EQ(ints_out[1], 20);
    EXPECT_EQ(ints_out[2], 30);

    // Inspect strs
    auto strs_out = p_data.template get_value<ParserDataType>("strs").template get<std::vector<std::string>>();
    ASSERT_EQ(strs_out.size(), 2u);
    EXPECT_EQ(strs_out[0], std::string("aa"));
    EXPECT_EQ(strs_out[1], std::string("bb"));

    // Inspect children
    auto children_arr = p_data.template get_value<ParserDataType>("children").template get<std::vector<ParserDataType>>();
    ASSERT_EQ(children_arr.size(), 2u);
    EXPECT_EQ(children_arr[0].template get_value<int32_t>("si32_test"), 5);
    EXPECT_EQ(children_arr[0].template get_value<std::string>("str_test"), std::string("aa"));
    EXPECT_EQ(children_arr[1].template get_value<int32_t>("si32_test"), 6);
    EXPECT_EQ(children_arr[1].template get_value<std::string>("str_test"), std::string("bb"));
}

template <typename ParserDataType>
inline void serializer_test_depth_2(ParserDataType p_data) {
    TestDepth2 d2{};
    d2.nested.si32_test = 11;
    d2.nested.str_test = "deep2";
    strcpy(d2.nested.buffer_test.buffer, "X");
    d2.depth2_id = 77;
    d2.depth2_name = "level2";

    WBE::SerializableSD<TestDepth2> sd;
    sd.serialize(p_data, d2);

    EXPECT_TRUE(p_data.contains("nested"));
    EXPECT_TRUE(p_data.contains("depth2_id"));
    EXPECT_TRUE(p_data.contains("depth2_name"));

    auto nested = p_data.template get_value<ParserDataType>("nested");
    EXPECT_EQ(nested.template get_value<int32_t>("si32_test"), 11);
    EXPECT_EQ(nested.template get_value<std::string>("str_test"), std::string("deep2"));
}

template <typename ParserDataType>
inline void serializer_test_depth_3(ParserDataType p_data) {
    TestDepth3 d3{};
    d3.nested2.nested.si32_test = 21;
    d3.nested2.nested.str_test = "deep3";
    d3.depth3_id = 88;
    d3.depth3_name = "level3";

    WBE::SerializableSD<TestDepth3> sd;
    sd.serialize(p_data, d3);

    EXPECT_TRUE(p_data.contains("nested2"));
    EXPECT_TRUE(p_data.contains("depth3_id"));

    auto n2 = p_data.template get_value<ParserDataType>("nested2");
    auto nested = n2.template get_value<ParserDataType>("nested");
    EXPECT_EQ(nested.template get_value<int32_t>("si32_test"), 21);
}

template <typename ParserDataType>
inline void serializer_vector_with_deep_nesting(ParserDataType p_data) {
    TestDeepVectorContainer container{};

    TestDepth2 d2a{}; d2a.nested.si32_test = 31; d2a.depth2_id = 1; d2a.depth2_name = "d2a";
    TestDepth2 d2b{}; d2b.nested.si32_test = 32; d2b.depth2_id = 2; d2b.depth2_name = "d2b";
    container.vec2.push_back(d2a);
    container.vec2.push_back(d2b);

    TestDepth3 d3a{}; d3a.nested2.nested.si32_test = 41; d3a.depth3_id = 3; d3a.depth3_name = "d3a";
    TestDepth3 d3b{}; d3b.nested2.nested.si32_test = 42; d3b.depth3_id = 4; d3b.depth3_name = "d3b";
    container.vec3.push_back(d3a);
    container.vec3.push_back(d3b);

    WBE::SerializableSD<TestDeepVectorContainer> sd;
    sd.serialize(p_data, container);

    EXPECT_TRUE(p_data.contains("vec2"));
    EXPECT_TRUE(p_data.contains("vec3"));

    auto out_vec2 = p_data.template get_value<ParserDataType>("vec2").template get<std::vector<ParserDataType>>();
    ASSERT_EQ(out_vec2.size(), 2u);
    EXPECT_EQ(out_vec2[0].template get_value<int32_t>("depth2_id"), 1);
    EXPECT_EQ(out_vec2[1].template get_value<int32_t>("depth2_id"), 2);

    auto out_vec3 = p_data.template get_value<ParserDataType>("vec3").template get<std::vector<ParserDataType>>();
    ASSERT_EQ(out_vec3.size(), 2u);
    EXPECT_EQ(out_vec3[0].template get_value<int32_t>("depth3_id"), 3);
    EXPECT_EQ(out_vec3[1].template get_value<int32_t>("depth3_id"), 4);
}

TEST_F(WBESerializerTest, DefaultConstruct) {
    WBE::JSONData json_data;
    serializer_test_default_construct(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_default_construct(yaml_data);
}

TEST_F(WBESerializerTest, General) {
    WBE::JSONData json_data;
    serializer_test_general(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_general(yaml_data);
}

TEST_F(WBESerializerTest, NestingSerialize) {
    WBE::JSONData json_data;
    serializer_test_nesting(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_nesting(yaml_data);
}

TEST_F(WBESerializerTest, DeepNestingSerialize) {
    WBE::JSONData json_data;
    serializer_test_deep_nesting(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_deep_nesting(yaml_data);
}

TEST_F(WBESerializerTest, VectorPrimitivesSerialize) {
    WBE::JSONData json_data_int, json_data_strs;
    serializer_test_vector_primitives(json_data_int, json_data_strs);
    WBE::YAMLData yaml_data_int, yaml_data_strs;
    serializer_test_vector_primitives(yaml_data_int, yaml_data_strs);
}

TEST_F(WBESerializerTest, VectorOfSerializableSerialize) {
    WBE::JSONData json_data;
    serializer_test_vector_serializables(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_vector_serializables(yaml_data);
}

TEST_F(WBESerializerTest, StructWithVectorFieldSerialize) {
    WBE::JSONData json_data;
    serializer_test_struct_with_vector_serializables(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_struct_with_vector_serializables(yaml_data);
}

TEST_F(WBESerializerTest, Depth2Serialize) {
    WBE::JSONData json_data;
    serializer_test_depth_2(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_depth_2(yaml_data);
}

TEST_F(WBESerializerTest, Depth3Serialize) {
    WBE::JSONData json_data;
    serializer_test_depth_3(json_data);
    WBE::YAMLData yaml_data;
    serializer_test_depth_3(yaml_data);
}

TEST_F(WBESerializerTest, VectorOfDeepNestingSerialize) {
    WBE::JSONData json_data;
    serializer_vector_with_deep_nesting(json_data);
    WBE::YAMLData yaml_data;
    serializer_vector_with_deep_nesting(yaml_data);
}


#endif
