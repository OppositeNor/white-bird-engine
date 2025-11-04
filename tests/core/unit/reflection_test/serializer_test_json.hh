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
#include "core/reflection/serializable.hh"
#include "global/global.hh"
#include "reflection_test_data.hh"
#include "generated/serializables_sd.gen.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace WBE = WhiteBirdEngine;

class WBESerializerJSONTest : public ::testing::Test {
protected:

    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }
 
    std::unique_ptr<WBE::Global> global;
};

TEST_F(WBESerializerJSONTest, DefaultConstruct) {
    TestSerializable serializable{};
    WBE::SerializableSD<TestSerializable> sd;
    WBE::JSONData data;
    sd.serialize(data, serializable);
    EXPECT_TRUE(data.contains("si32_test"));
    EXPECT_TRUE(data.contains("si64_test"));
    EXPECT_TRUE(data.contains("ui32_test"));
    EXPECT_TRUE(data.contains("ui64_test"));
    EXPECT_TRUE(data.contains("f32_test"));
    EXPECT_TRUE(data.contains("f64_test"));
    EXPECT_TRUE(data.contains("vec3_test"));
    EXPECT_TRUE(data.contains("vec4_test"));
    EXPECT_TRUE(data.contains("str_test"));
    EXPECT_TRUE(data.contains("buffer_test"));
    EXPECT_EQ(data.get_value<int32_t>("si32_test"), 0);
    EXPECT_EQ(data.get_value<int64_t>("si64_test"), 0);
    EXPECT_EQ(data.get_value<uint32_t>("ui32_test"), 0);
    EXPECT_EQ(data.get_value<uint64_t>("ui64_test"), 0);
    EXPECT_FLOAT_EQ(data.get_value<float>("f32_test"), 0.0f);
    EXPECT_DOUBLE_EQ(data.get_value<double>("f64_test"), 0.0);
    EXPECT_EQ(data.get_value<glm::vec3>("vec3_test"), glm::vec3());
    EXPECT_EQ(data.get_value<glm::vec4>("vec4_test"), glm::vec4());
    EXPECT_EQ(data.get_value<std::string>("str_test"), std::string());
}


TEST_F(WBESerializerJSONTest, General) {
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
    WBE::JSONData data;
    sd.serialize(data, serializable);

    // verify keys and values
    EXPECT_TRUE(data.contains("si32_test"));
    EXPECT_TRUE(data.contains("si64_test"));
    EXPECT_TRUE(data.contains("ui32_test"));
    EXPECT_TRUE(data.contains("ui64_test"));
    EXPECT_TRUE(data.contains("f32_test"));
    EXPECT_TRUE(data.contains("f64_test"));
    EXPECT_TRUE(data.contains("vec3_test"));
    EXPECT_TRUE(data.contains("vec4_test"));
    EXPECT_TRUE(data.contains("str_test"));
    EXPECT_TRUE(data.contains("buffer_test"));

    EXPECT_EQ(data.get_value<int32_t>("si32_test"), 3);
    EXPECT_EQ(data.get_value<int64_t>("si64_test"), -62);
    EXPECT_EQ(data.get_value<uint32_t>("ui32_test"), 42u);
    EXPECT_EQ(data.get_value<uint64_t>("ui64_test"), 59u);
    EXPECT_FLOAT_EQ(data.get_value<float>("f32_test"), 3.14f);
    EXPECT_DOUBLE_EQ(data.get_value<double>("f64_test"), 2.718);
    EXPECT_EQ(data.get_value<glm::vec3>("vec3_test"), glm::vec3(1, -2, 3));
    EXPECT_EQ(data.get_value<glm::vec4>("vec4_test"), glm::vec4(1, -2, 3, -4));
    EXPECT_EQ(data.get_value<std::string>("str_test"), std::string("Hello!"));
    EXPECT_EQ(data.get_value<std::string>("buffer_test"), std::string("how are you?"));
}

TEST_F(WBESerializerJSONTest, NestingSerialize) {
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
    WBE::JSONData data;
    sd.serialize(data, serializable);

    // Top-level checks
    EXPECT_TRUE(data.contains("nesting_id"));
    EXPECT_TRUE(data.contains("name"));
    EXPECT_TRUE(data.contains("small_buffer"));
    EXPECT_TRUE(data.contains("nested_test"));

    EXPECT_EQ(data.get_value<int32_t>("nesting_id"), 7);
    EXPECT_EQ(data.get_value<std::string>("name"), std::string("parent"));
    EXPECT_EQ(data.get_value<std::string>("small_buffer"), std::string("abc"));

    // Nested checks
    auto nested = data.get_value<WBE::JSONData>("nested_test");
    EXPECT_EQ(nested.get_value<int32_t>("si32_test"), 3);
    EXPECT_EQ(nested.get_value<int64_t>("si64_test"), -62);
    EXPECT_EQ(nested.get_value<uint32_t>("ui32_test"), 42u);
    EXPECT_EQ(nested.get_value<uint64_t>("ui64_test"), 59u);
    EXPECT_FLOAT_EQ(nested.get_value<float>("f32_test"), 3.14f);
    EXPECT_DOUBLE_EQ(nested.get_value<double>("f64_test"), 2.718);
    EXPECT_EQ(nested.get_value<glm::vec3>("vec3_test"), glm::vec3(1, -2, 3));
    EXPECT_EQ(nested.get_value<glm::vec4>("vec4_test"), glm::vec4(1, -2, 3, -4));
    EXPECT_EQ(nested.get_value<std::string>("str_test"), std::string("Hello!"));
    EXPECT_EQ(nested.get_value<std::string>("buffer_test"), std::string("how are you?"));
}

TEST_F(WBESerializerJSONTest, NestingRoundTrip) {
    TestSerializableNesting serializable{};
    // populate same as above
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

    // Serialize
    WBE::SerializableSD<TestSerializableNesting> sd;
    WBE::JSONData data;
    sd.serialize(data, serializable);

    // Deserialize into a fresh object and compare
    TestSerializableNesting out{};
    sd.deserialize(data, out);

    EXPECT_EQ(out.nesting_id, serializable.nesting_id);
    EXPECT_EQ(out.name, serializable.name);
    EXPECT_STREQ(out.small_buffer.buffer, serializable.small_buffer.buffer);

    EXPECT_EQ(out.nested_test.si32_test, serializable.nested_test.si32_test);
    EXPECT_EQ(out.nested_test.si64_test, serializable.nested_test.si64_test);
    EXPECT_EQ(out.nested_test.ui32_test, serializable.nested_test.ui32_test);
    EXPECT_EQ(out.nested_test.ui64_test, serializable.nested_test.ui64_test);
    EXPECT_FLOAT_EQ(out.nested_test.f32_test, serializable.nested_test.f32_test);
    EXPECT_DOUBLE_EQ(out.nested_test.f64_test, serializable.nested_test.f64_test);
    EXPECT_EQ(out.nested_test.vec3_test, serializable.nested_test.vec3_test);
    EXPECT_EQ(out.nested_test.vec4_test, serializable.nested_test.vec4_test);
    EXPECT_EQ(out.nested_test.str_test, serializable.nested_test.str_test);
    EXPECT_STREQ(out.nested_test.buffer_test.buffer, serializable.nested_test.buffer_test.buffer);
}

TEST_F(WBESerializerJSONTest, DeepNestingSerialize) {
    // Depth-2: TestDepth2 contains TestSerializable
    TestDepth2 td2{};
    td2.depth2_id = 123;
    td2.depth2_name = "depth2";
    td2.nested.si32_test = 9;
    td2.nested.str_test = "deep";
    strcpy(td2.nested.buffer_test.buffer, "B2");

    WBE::SerializableSD<TestDepth2> sd;
    WBE::JSONData data;
    sd.serialize(data, td2);

    EXPECT_TRUE(data.contains("nested"));
    EXPECT_TRUE(data.contains("depth2_id"));
    EXPECT_TRUE(data.contains("depth2_name"));

    auto nested = data.get_value<WBE::JSONData>("nested");
    EXPECT_EQ(nested.get_value<int32_t>("si32_test"), 9);
    EXPECT_EQ(nested.get_value<std::string>("str_test"), std::string("deep"));
    EXPECT_EQ(nested.get_value<std::string>("buffer_test"), std::string("B2"));
}

TEST_F(WBESerializerJSONTest, DeepNestingRoundTrip) {
    // Depth-3: TestDepth3 contains TestDepth2 which contains TestSerializable
    TestDepth3 td3{};
    td3.depth3_id = 777;
    td3.depth3_name = "depth3";
    td3.nested2.depth2_id = 456;
    td3.nested2.depth2_name = "inner2";
    td3.nested2.nested.si32_test = -10;
    td3.nested2.nested.str_test = "innermost";
    strcpy(td3.nested2.nested.buffer_test.buffer, "D3");

    WBE::SerializableSD<TestDepth3> sd;
    WBE::JSONData data;
    sd.serialize(data, td3);

    // Deserialize and compare
    TestDepth3 out{};
    sd.deserialize(data, out);

    EXPECT_EQ(out.depth3_id, td3.depth3_id);
    EXPECT_EQ(out.depth3_name, td3.depth3_name);
    EXPECT_EQ(out.nested2.depth2_id, td3.nested2.depth2_id);
    EXPECT_EQ(out.nested2.depth2_name, td3.nested2.depth2_name);
    EXPECT_EQ(out.nested2.nested.si32_test, td3.nested2.nested.si32_test);
    EXPECT_EQ(out.nested2.nested.str_test, td3.nested2.nested.str_test);
    EXPECT_STREQ(out.nested2.nested.buffer_test.buffer, td3.nested2.nested.buffer_test.buffer);
}

TEST_F(WBESerializerJSONTest, VectorPrimitivesSerialize) {
    // Test serializing vectors of primitives
    std::vector<int> ints = {1, 2, 3, 4, 5};
    WBE::SerializableSD<std::vector<int>> sd_ints;
    WBE::JSONData data_ints;
    sd_ints.serialize(data_ints, ints);

    auto out_ints = data_ints.get<std::vector<int>>();
    ASSERT_EQ(out_ints.size(), ints.size());
    for (size_t i = 0; i < ints.size(); ++i) EXPECT_EQ(out_ints[i], ints[i]);

    // Test vector<string>
    std::vector<std::string> strs = {"one", "two", "three"};
    WBE::SerializableSD<std::vector<std::string>> sd_strs;
    WBE::JSONData data_strs;
    sd_strs.serialize(data_strs, strs);

    auto out_strs = data_strs.get<std::vector<std::string>>();
    ASSERT_EQ(out_strs.size(), strs.size());
    for (size_t i = 0; i < strs.size(); ++i) EXPECT_EQ(out_strs[i], strs[i]);
}

TEST_F(WBESerializerJSONTest, VectorOfSerializableSerialize) {
    // Test serializing vector of TestSerializable objects
    std::vector<TestSerializable> vec;
    TestSerializable a; a.si32_test = 1; a.str_test = "a"; strcpy(a.buffer_test.buffer, "A");
    TestSerializable b; b.si32_test = 2; b.str_test = "b"; strcpy(b.buffer_test.buffer, "B");
    vec.push_back(a);
    vec.push_back(b);

    WBE::SerializableSD<std::vector<TestSerializable>> sd_vec;
    WBE::JSONData data;
    sd_vec.serialize(data, vec);

    // Expect an array of objects
    auto arr = data.get<std::vector<WBE::JSONData>>();
    ASSERT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0].get_value<int32_t>("si32_test"), 1);
    EXPECT_EQ(arr[0].get_value<std::string>("str_test"), std::string("a"));
    EXPECT_EQ(arr[1].get_value<int32_t>("si32_test"), 2);
    EXPECT_EQ(arr[1].get_value<std::string>("str_test"), std::string("b"));
}

TEST_F(WBESerializerJSONTest, StructWithVectorFieldSerialize) {
    // Build a container with vectors
    TestVectorContainer container{};
    container.ints = {10, 20, 30};
    container.strs = {"aa", "bb"};

    TestSerializable a; a.si32_test = 5; a.str_test = "aa"; strcpy(a.buffer_test.buffer, "A");
    TestSerializable b; b.si32_test = 6; b.str_test = "bb"; strcpy(b.buffer_test.buffer, "B");
    container.children.push_back(a);
    container.children.push_back(b);

    WBE::SerializableSD<TestVectorContainer> sd;
    WBE::JSONData data;
    sd.serialize(data, container);

    // Top-level fields
    EXPECT_TRUE(data.contains("ints"));
    EXPECT_TRUE(data.contains("strs"));
    EXPECT_TRUE(data.contains("children"));

    // Inspect ints
    auto ints_out = data.get_value<WBE::JSONData>("ints").get<std::vector<int>>();
    ASSERT_EQ(ints_out.size(), 3u);
    EXPECT_EQ(ints_out[0], 10);
    EXPECT_EQ(ints_out[1], 20);
    EXPECT_EQ(ints_out[2], 30);

    // Inspect strs
    auto strs_out = data.get_value<WBE::JSONData>("strs").get<std::vector<std::string>>();
    ASSERT_EQ(strs_out.size(), 2u);
    EXPECT_EQ(strs_out[0], std::string("aa"));
    EXPECT_EQ(strs_out[1], std::string("bb"));

    // Inspect children
    auto children_arr = data.get_value<WBE::JSONData>("children").get<std::vector<WBE::JSONData>>();
    ASSERT_EQ(children_arr.size(), 2u);
    EXPECT_EQ(children_arr[0].get_value<int32_t>("si32_test"), 5);
    EXPECT_EQ(children_arr[0].get_value<std::string>("str_test"), std::string("aa"));
    EXPECT_EQ(children_arr[1].get_value<int32_t>("si32_test"), 6);
    EXPECT_EQ(children_arr[1].get_value<std::string>("str_test"), std::string("bb"));
}

TEST_F(WBESerializerJSONTest, Depth2Serialize) {
    TestDepth2 d2{};
    d2.nested.si32_test = 11;
    d2.nested.str_test = "deep2";
    strcpy(d2.nested.buffer_test.buffer, "X");
    d2.depth2_id = 77;
    d2.depth2_name = "level2";

    WBE::SerializableSD<TestDepth2> sd;
    WBE::JSONData data;
    sd.serialize(data, d2);

    EXPECT_TRUE(data.contains("nested"));
    EXPECT_TRUE(data.contains("depth2_id"));
    EXPECT_TRUE(data.contains("depth2_name"));

    auto nested = data.get_value<WBE::JSONData>("nested");
    EXPECT_EQ(nested.get_value<int32_t>("si32_test"), 11);
    EXPECT_EQ(nested.get_value<std::string>("str_test"), std::string("deep2"));
}

TEST_F(WBESerializerJSONTest, Depth3Serialize) {
    TestDepth3 d3{};
    d3.nested2.nested.si32_test = 21;
    d3.nested2.nested.str_test = "deep3";
    d3.depth3_id = 88;
    d3.depth3_name = "level3";

    WBE::SerializableSD<TestDepth3> sd;
    WBE::JSONData data;
    sd.serialize(data, d3);

    EXPECT_TRUE(data.contains("nested2"));
    EXPECT_TRUE(data.contains("depth3_id"));

    auto n2 = data.get_value<WBE::JSONData>("nested2");
    auto nested = n2.get_value<WBE::JSONData>("nested");
    EXPECT_EQ(nested.get_value<int32_t>("si32_test"), 21);
}

TEST_F(WBESerializerJSONTest, VectorOfDeepNestingSerialize) {
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
    WBE::JSONData data;
    sd.serialize(data, container);

    EXPECT_TRUE(data.contains("vec2"));
    EXPECT_TRUE(data.contains("vec3"));

    auto out_vec2 = data.get_value<WBE::JSONData>("vec2").get<std::vector<WBE::JSONData>>();
    ASSERT_EQ(out_vec2.size(), 2u);
    EXPECT_EQ(out_vec2[0].get_value<int32_t>("depth2_id"), 1);
    EXPECT_EQ(out_vec2[1].get_value<int32_t>("depth2_id"), 2);

    auto out_vec3 = data.get_value<WBE::JSONData>("vec3").get<std::vector<WBE::JSONData>>();
    ASSERT_EQ(out_vec3.size(), 2u);
    EXPECT_EQ(out_vec3[0].get_value<int32_t>("depth3_id"), 3);
    EXPECT_EQ(out_vec3[1].get_value<int32_t>("depth3_id"), 4);
}


#endif
