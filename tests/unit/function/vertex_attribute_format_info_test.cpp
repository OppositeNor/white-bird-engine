/* Copyright 2026 OppositeNor

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
#include "function/render/render_resource_unit.hh"
#include "function/render/vertex_attribute_format_info.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

TEST(WBEVertexAttributeFormatInfoTest, ReturnsTightlyPackedSizeOfEveryFormat) {
    const std::array<std::pair<RUVertexAttributeFormat, size_t>, 12> expected_sizes{{
        {RUVertexAttributeFormat::FLOAT, 4},
        {RUVertexAttributeFormat::FLOAT2, 8},
        {RUVertexAttributeFormat::FLOAT3, 12},
        {RUVertexAttributeFormat::FLOAT4, 16},
        {RUVertexAttributeFormat::INT, 4},
        {RUVertexAttributeFormat::INT2, 8},
        {RUVertexAttributeFormat::INT3, 12},
        {RUVertexAttributeFormat::INT4, 16},
        {RUVertexAttributeFormat::UINT, 4},
        {RUVertexAttributeFormat::UINT2, 8},
        {RUVertexAttributeFormat::UINT3, 12},
        {RUVertexAttributeFormat::UINT4, 16},
    }};

    for (const auto& [format, size] : expected_sizes) {
        EXPECT_EQ(VertexAttributeFormatInfo::get_size(format), size) << static_cast<int>(format);
    }
}

TEST(WBEVertexAttributeFormatInfoTest, ThrowsForUnsupportedFormat) {
    const auto unsupported = static_cast<RUVertexAttributeFormat>(UINT8_MAX);

    EXPECT_THROW((void)VertexAttributeFormatInfo::get_size(unsupported), std::runtime_error);
}

} // namespace WhiteBirdEngine
