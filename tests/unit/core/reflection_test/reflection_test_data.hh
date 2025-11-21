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
WBE_STRUCT(TestEmptySerializable, WBE_SERIALIZABLE) final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestEmptySerializable)
};

WBE_STRUCT(TestSerializable, WBE_SERIALIZABLE) final : public Serializable {
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

WBE_STRUCT(TestPartialStruct, WBE_SERIALIZABLE) final : public Serializable {
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

WBE_STRUCT(TestSerializableNesting, WBE_SERIALIZABLE) final : public Serializable {
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
WBE_STRUCT(TestVectorContainer, WBE_SERIALIZABLE) final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestVectorContainer)

    WBE_META(WBE_REFLECT)
    std::vector<int> ints;
    WBE_META(WBE_REFLECT)
    std::vector<std::string> strs;
    WBE_META(WBE_REFLECT)
    std::vector<TestSerializable> children;
};

// Depth-2 nested struct: contains a TestSerializable
WBE_STRUCT(TestDepth2, WBE_SERIALIZABLE) final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDepth2)

    WBE_META(WBE_REFLECT)
    TestSerializable nested;
    WBE_META(WBE_REFLECT)
    int32_t depth2_id = 0;
    WBE_META(WBE_REFLECT)
    std::string depth2_name;
};

// Depth-3 nested struct: contains a TestDepth2
WBE_STRUCT(TestDepth3, WBE_SERIALIZABLE) final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDepth3)
    WBE_META(WBE_REFLECT)
    TestDepth2 nested2;
    WBE_META(WBE_REFLECT)
    int32_t depth3_id = 0;
    WBE_META(WBE_REFLECT)
    std::string depth3_name;
};

// Container for vectors of deep structs
WBE_STRUCT(TestDeepVectorContainer, WBE_SERIALIZABLE) final : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDeepVectorContainer)

    WBE_META(WBE_REFLECT)
    std::vector<TestDepth2> vec2;
    WBE_META(WBE_REFLECT)
    std::vector<TestDepth3> vec3;
};

// Inheritance test structures
WBE_STRUCT(TestInheritedBase, WBE_SERIALIZABLE) : public Serializable {
    WBE_DECL_SERIALIZABLE(TestInheritedBase)

    WBE_META(WBE_REFLECT)
    int32_t base_id = 0;
    WBE_META(WBE_REFLECT)
    std::string base_name;
    WBE_META(WBE_REFLECT)
    float base_value = 0.0f;
};

WBE_STRUCT(TestInheritedChild, WBE_SERIALIZABLE) : public TestInheritedBase {
    WBE_DECL_SERIALIZABLE(TestInheritedChild)

    WBE_META(WBE_REFLECT)
    int32_t child_id = 0;
    WBE_META(WBE_REFLECT)
    std::string child_name;
    WBE_META(WBE_REFLECT)
    double child_value = 0.0;
};

// Multi-level inheritance test
WBE_STRUCT(TestInheritedGrandchild, WBE_SERIALIZABLE) : public TestInheritedChild {
    WBE_DECL_SERIALIZABLE(TestInheritedGrandchild)

    WBE_META(WBE_REFLECT)
    int32_t grandchild_id = 0;
    WBE_META(WBE_REFLECT)
    std::string grandchild_name;
    WBE_META(WBE_REFLECT)
    glm::vec3 grandchild_vector;
};

// Inheritance with nested serializable objects
WBE_STRUCT(TestInheritedWithNested, WBE_SERIALIZABLE) : public TestInheritedBase {
    WBE_DECL_SERIALIZABLE(TestInheritedWithNested)

    WBE_META(WBE_REFLECT)
    TestSerializable nested_object;
    WBE_META(WBE_REFLECT)
    std::vector<int> child_numbers;
};

// Multiple inheritance test structures
WBE_STRUCT(TestMultipleInheritanceA, WBE_SERIALIZABLE) : public Serializable {
    WBE_DECL_SERIALIZABLE(TestMultipleInheritanceA)

    WBE_META(WBE_REFLECT)
    int32_t a_id = 0;
    WBE_META(WBE_REFLECT)
    std::string a_name;
    WBE_META(WBE_REFLECT)
    float a_value = 0.0f;
};

WBE_STRUCT(TestMultipleInheritanceB, WBE_SERIALIZABLE) : public Serializable {
    WBE_DECL_SERIALIZABLE(TestMultipleInheritanceB)
    WBE_META(WBE_REFLECT)
    int32_t b_id = 0;
    WBE_META(WBE_REFLECT)
    std::string b_name;
    WBE_META(WBE_REFLECT)
    double b_value = 0.0;
};

WBE_STRUCT(TestMultipleInheritanceChild, WBE_SERIALIZABLE) final : public TestMultipleInheritanceA, public TestMultipleInheritanceB {
    WBE_DECL_SERIALIZABLE(TestMultipleInheritanceChild)

    WBE_META(WBE_REFLECT)
    int32_t child_id = 0;
    WBE_META(WBE_REFLECT)
    std::string child_name;
    WBE_META(WBE_REFLECT)
    glm::vec2 child_vector;
};

// Diamond inheritance pattern test
WBE_STRUCT(TestDiamondBase, WBE_SERIALIZABLE) : public Serializable {
    WBE_DECL_SERIALIZABLE(TestDiamondBase)

    WBE_META(WBE_REFLECT)
    int32_t diamond_base_id = 0;
    WBE_META(WBE_REFLECT)
    std::string diamond_base_name;
};

WBE_STRUCT(TestDiamondLeft, WBE_SERIALIZABLE) : virtual public TestDiamondBase {
    WBE_DECL_SERIALIZABLE(TestDiamondLeft)

    WBE_META(WBE_REFLECT)
    int32_t left_id = 0;
    WBE_META(WBE_REFLECT)
    float left_value = 0.0f;
};

WBE_STRUCT(TestDiamondRight, WBE_SERIALIZABLE) : virtual public TestDiamondBase {
    WBE_DECL_SERIALIZABLE(TestDiamondRight)

    WBE_META(WBE_REFLECT)
    int32_t right_id = 0;
    WBE_META(WBE_REFLECT)
    double right_value = 0.0;
};

WBE_STRUCT(TestDiamondChild, WBE_SERIALIZABLE) final : public TestDiamondLeft, public TestDiamondRight {
    WBE_DECL_SERIALIZABLE(TestDiamondChild)

    WBE_META(WBE_REFLECT)
    int32_t diamond_child_id = 0;
    WBE_META(WBE_REFLECT)
    std::string diamond_child_name;
};

}

#endif
