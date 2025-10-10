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
