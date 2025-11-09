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
#include "utils/utils.hh"
#include <glm/glm.hpp>
#include <string>

namespace WBE = WhiteBirdEngine;

WBE_STRUCT(TestEmptySerializable, WBE_SERIALIZABLE) {
};

WBE_STRUCT(TestSerializable, WBE_SERIALIZABLE) {
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
    WBE::Buffer<16> buffer_test;
};

WBE_STRUCT(TestPartialStruct, WBE_SERIALIZABLE) {
    WBE_META(WBE_REFLECT)
    int32_t  si32_test;
    WBE_META(WBE_REFLECT)
    int64_t  si64_test;
    WBE_META(WBE_REFLECT)
    uint32_t ui32_test;
    WBE_META(WBE_REFLECT)
    uint64_t ui64_test;
};

WBE_STRUCT(TestSerializableNesting, WBE_SERIALIZABLE) {
    WBE_META(WBE_REFLECT)
    TestSerializable nested_test;
    WBE_META(WBE_REFLECT)
    int32_t nesting_id;
    WBE_META(WBE_REFLECT)
    std::string name;
    WBE_META(WBE_REFLECT)
    WBE::Buffer<8> small_buffer;
};

// A simple container struct that holds vectors to test vector serialization
WBE_STRUCT(TestVectorContainer, WBE_SERIALIZABLE) {
    WBE_META(WBE_REFLECT)
    std::vector<int> ints;
    WBE_META(WBE_REFLECT)
    std::vector<std::string> strs;
    WBE_META(WBE_REFLECT)
    std::vector<TestSerializable> children;
};

// Depth-2 nested struct: contains a TestSerializable
WBE_STRUCT(TestDepth2, WBE_SERIALIZABLE) {
    WBE_META(WBE_REFLECT)
    TestSerializable nested;
    WBE_META(WBE_REFLECT)
    int32_t depth2_id = 0;
    WBE_META(WBE_REFLECT)
    std::string depth2_name;
};

// Depth-3 nested struct: contains a TestDepth2
WBE_STRUCT(TestDepth3, WBE_SERIALIZABLE) {
    WBE_META(WBE_REFLECT)
    TestDepth2 nested2;
    WBE_META(WBE_REFLECT)
    int32_t depth3_id = 0;
    WBE_META(WBE_REFLECT)
    std::string depth3_name;
};

// Container for vectors of deep structs
WBE_STRUCT(TestDeepVectorContainer, WBE_SERIALIZABLE) {
    WBE_META(WBE_REFLECT)
    std::vector<TestDepth2> vec2;
    WBE_META(WBE_REFLECT)
    std::vector<TestDepth3> vec3;
};

#endif
