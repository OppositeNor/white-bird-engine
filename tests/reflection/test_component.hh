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
#ifndef __WBE_TEST_COMPONENT_HH__
#define __WBE_TEST_COMPONENT_HH__
#include "core/reflection/reflection_defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>

namespace WBE = WhiteBirdEngine;

WBE_STRUCT(TestComponent, WBE_COMPONENT) {
    // Test for signed integers.
    WBE_META(WBE_REFLECT)
    int32_t sinteger;
    // Test for unsigned integers.
    WBE_META(WBE_REFLECT)
    uint32_t uinteger;
    // Test for float numbers.
    WBE_META(WBE_REFLECT)
    float float_num;
    // Test for double numbers.
    WBE_META(WBE_REFLECT)
    double double_num;
    // Test for buffer.
    static constexpr size_t MAX_BUFFER_STR_SIZE = 15;
    WBE_META(WBE_REFLECT)
    WBE::Buffer<MAX_BUFFER_STR_SIZE> buffer_string_test;
    // Test for string
    WBE_META(WBE_REFLECT)
    std::string string_test;
};


#endif
