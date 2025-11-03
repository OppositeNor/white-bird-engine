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
#include "test_deserializer_data.hh"
#include "generated/serializables_sd.gen.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "utils/utils.hh"
#include <glm/glm.hpp>
#include <string>
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

class WBEDeserializerTest : public ::testing::Test {
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
};

TEST_F(WBEDeserializerTest, General) {
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
}

#endif
