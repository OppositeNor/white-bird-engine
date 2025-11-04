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
#ifndef __WBE_ACCESSOR_TEST_HH__
#define __WBE_ACCESSOR_TEST_HH__

#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "reflection_test_data.hh"
#include "generated/serializables_access.gen.hh"

#include <cstdint>
#include <glm/glm.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

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
};

TEST_F(WBEDeserializerTest, General) {
    TestSerializable serializable;
    serializable = {
        .si32_test = 1,
        .si64_test = -2,
        .ui32_test = 42,
        .ui64_test = 365,
        .f32_test = 3.14,
        .f64_test = 2.718,
        .vec3_test = glm::vec3(3, 2, 1),
        .vec4_test = glm::vec4(1, 2, 3, 4),
        .str_test = "Hello, world!"
    };
    WBE::SerializableAccess<TestSerializable> accessor;
    EXPECT_EQ(accessor.get_field<int32_t>(serializable, "si32_test"), 1);
    EXPECT_EQ(accessor.get_field<int64_t>(serializable, "si64_test"), -2);
    EXPECT_EQ(accessor.get_field<uint32_t>(serializable, "ui32_test"), 42);
    EXPECT_EQ(accessor.get_field<uint64_t>(serializable, "ui64_test"), 365);
    ASSERT_THROW(accessor.get_field<uint32_t>(serializable, "some_field"), std::runtime_error);
}


#endif
