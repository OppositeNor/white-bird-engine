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

#ifndef __WBE_DESERIALIZER_TEST_HH__
#define __WBE_DESERIALIZER_TEST_HH__
#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"
#include "utils/utils.hh"
#include <glm/glm.hpp>
#include <string>

namespace WhiteBirdEngine {
struct WBE_META(WBE_SERIALIZABLE) TestEmptySerializable final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestEmptySerializable)
};

struct WBE_META(WBE_SERIALIZABLE) TestSerializable final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestSerializable)

    WBE_META(WBE_REFLECT)
    int32_t  si32_test;
    WBE_META(WBE_REFLECT)
    int64_t  si64_test;
    WBE_META(WBE_REFLECT)
    uint32_t ui32_test;
    WBE_META(WBE_REFLECT)
    uint64_t ui64_test;
    WBE_META(WBE_REFLECT)
    float    f32_test;
    WBE_META(WBE_REFLECT)
    double   f64_test;
    WBE_META(WBE_REFLECT)
    glm::vec3 vec3_test;
    WBE_META(WBE_REFLECT)
    glm::vec4 vec4_test;
    WBE_META(WBE_REFLECT)
    std::string str_test;
    WBE_META(WBE_REFLECT)
    Buffer<16> buffer_test;
};

struct WBE_META(WBE_SERIALIZABLE) TestPartialStruct final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestPartialStruct)

    WBE_META(WBE_REFLECT)
    int32_t  si32_test;
    WBE_META(WBE_REFLECT)
    int64_t  si64_test;
    WBE_META(WBE_REFLECT)
    uint32_t ui32_test;
    WBE_META(WBE_REFLECT)
    uint64_t ui64_test;
};

struct WBE_META(WBE_SERIALIZABLE) TestSerializableNesting final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestSerializableNesting)

    WBE_META(WBE_REFLECT)
    TestSerializable nested_test;
    WBE_META(WBE_REFLECT)
    int32_t nesting_id;
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    Buffer<8> small_buffer;
};

// A simple container struct that holds vectors to test vector serialization
struct WBE_META(WBE_SERIALIZABLE) TestVectorContainer final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestVectorContainer)

    WBE_META(WBE_REFLECT)
    std::vector<int> ints;
    WBE_META(WBE_REFLECT)
    std::vector<std::string> strs;
    WBE_META(WBE_REFLECT)
    std::vector<TestSerializable> children;
};

// Depth-2 nested struct: contains a TestSerializable
struct WBE_META(WBE_SERIALIZABLE) TestDepth2 final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDepth2)

    WBE_META(WBE_REFLECT)
    TestSerializable nested;
    WBE_META(WBE_REFLECT)
    int32_t depth2_id = 0;
    WBE_META(WBE_REFLECT)
    std::string depth2_name;
};

// Depth-3 nested struct: contains a TestDepth2
struct WBE_META(WBE_SERIALIZABLE) TestDepth3 final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDepth3)
    WBE_META(WBE_REFLECT)
    TestDepth2 nested2;
    WBE_META(WBE_REFLECT)
    int32_t depth3_id = 0;
    WBE_META(WBE_REFLECT)
    std::string depth3_name;
};

// Container for vectors of deep structs
struct WBE_META(WBE_SERIALIZABLE) TestDeepVectorContainer final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDeepVectorContainer)

    WBE_META(WBE_REFLECT)
    std::vector<TestDepth2> vec2;
    WBE_META(WBE_REFLECT)
    std::vector<TestDepth3> vec3;
};

// Inheritance test structures
struct WBE_META(WBE_SERIALIZABLE) TestInheritedBase : public Serializable {
    WBE_DECL_SERIALIZABLE(TestInheritedBase)

    WBE_META(WBE_REFLECT)
    int32_t base_id = 0;
    WBE_META(WBE_REFLECT)
    std::string base_name;
    WBE_META(WBE_REFLECT)
    float base_value = 0.0f;
};

struct WBE_META(WBE_SERIALIZABLE) TestInheritedChild : public TestInheritedBase {
    WBE_DECL_SERIALIZABLE(TestInheritedChild)

    WBE_META(WBE_REFLECT)
    int32_t child_id = 0;
    WBE_META(WBE_REFLECT)
    std::string child_name;
    WBE_META(WBE_REFLECT)
    double child_value = 0.0;
};

// Multi-level inheritance test
struct WBE_META(WBE_SERIALIZABLE) TestInheritedGrandchild : public TestInheritedChild {
    WBE_DECL_SERIALIZABLE(TestInheritedGrandchild)

    WBE_META(WBE_REFLECT)
    int32_t grandchild_id = 0;
    WBE_META(WBE_REFLECT)
    std::string grandchild_name;
    WBE_META(WBE_REFLECT)
    glm::vec3 grandchild_vector;
};

// Inheritance with nested serializable objects
struct WBE_META(WBE_SERIALIZABLE) TestInheritedWithNested : public TestInheritedBase {
    WBE_DECL_SERIALIZABLE(TestInheritedWithNested)

    WBE_META(WBE_REFLECT)
    TestSerializable nested_object;
    WBE_META(WBE_REFLECT)
    std::vector<int> child_numbers;
};

// Multiple inheritance test structures
struct WBE_META(WBE_SERIALIZABLE) TestMultipleInheritanceA : public Serializable {
    WBE_DECL_SERIALIZABLE(TestMultipleInheritanceA)

    WBE_META(WBE_REFLECT)
    int32_t a_id = 0;
    WBE_META(WBE_REFLECT)
    std::string a_name;
    WBE_META(WBE_REFLECT)
    float a_value = 0.0f;
};

struct WBE_META(WBE_SERIALIZABLE) TestMultipleInheritanceB : public Serializable {
    WBE_DECL_SERIALIZABLE(TestMultipleInheritanceB)
    WBE_META(WBE_REFLECT)
    int32_t b_id = 0;
    WBE_META(WBE_REFLECT)
    std::string b_name;
    WBE_META(WBE_REFLECT)
    double b_value = 0.0;
};

struct WBE_META(WBE_SERIALIZABLE) TestMultipleInheritanceChild final : public TestMultipleInheritanceA, public TestMultipleInheritanceB {
    WBE_DECL_SERIALIZABLE(TestMultipleInheritanceChild)

    WBE_META(WBE_REFLECT)
    int32_t child_id = 0;
    WBE_META(WBE_REFLECT)
    std::string child_name;
    WBE_META(WBE_REFLECT)
    glm::vec2 child_vector;
};

// Diamond inheritance pattern test
struct WBE_META(WBE_SERIALIZABLE) TestDiamondBase : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDiamondBase)

    WBE_META(WBE_REFLECT)
    int32_t diamond_base_id = 0;
    WBE_META(WBE_REFLECT)
    std::string diamond_base_name;
};

struct WBE_META(WBE_SERIALIZABLE) TestDiamondLeft : virtual public TestDiamondBase {
    WBE_DECL_SERIALIZABLE(TestDiamondLeft)

    WBE_META(WBE_REFLECT)
    int32_t left_id = 0;
    WBE_META(WBE_REFLECT)
    float left_value = 0.0f;
};

struct WBE_META(WBE_SERIALIZABLE) TestDiamondRight : virtual public TestDiamondBase {
    WBE_DECL_SERIALIZABLE(TestDiamondRight)

    WBE_META(WBE_REFLECT)
    int32_t right_id = 0;
    WBE_META(WBE_REFLECT)
    double right_value = 0.0;
};

struct WBE_META(WBE_SERIALIZABLE) TestDiamondChild final : public TestDiamondLeft, public TestDiamondRight {
    WBE_DECL_SERIALIZABLE(TestDiamondChild)

    WBE_META(WBE_REFLECT)
    int32_t diamond_child_id = 0;
    WBE_META(WBE_REFLECT)
    std::string diamond_child_name;
};

}

#endif
