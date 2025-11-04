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
}


#endif
