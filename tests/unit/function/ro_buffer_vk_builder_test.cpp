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
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/function_utils.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>

namespace WhiteBirdEngine {

TEST(WBEROBufferVKBuilderTest, BuildConfiguresSingleElementWithDefaultDomain) {
    ROBufferVK::Builder builder;

    ROBufferVK::Builder& result = builder.build(64);

    EXPECT_EQ(&result, &builder);
    EXPECT_EQ(builder.size, 64);
    EXPECT_EQ(WBE_REQUIRED(builder.domain), DeviceDomain::HOST_DEVICE);
}

TEST(WBEROBufferVKBuilderTest, BuildConfiguresElementArrayAndExplicitDomain) {
    ROBufferVK::Builder builder;

    builder.build(16, 8, DeviceDomain::DEVICE);

    EXPECT_EQ(builder.size, 128);
    EXPECT_EQ(WBE_REQUIRED(builder.domain), DeviceDomain::DEVICE);
}

TEST(WBEROBufferVKBuilderTest, BuildRejectsInvalidSizes) {
    ROBufferVK::Builder builder;

    EXPECT_THROW(builder.build(0), std::runtime_error);
    EXPECT_THROW(builder.build(1, 0), std::runtime_error);
    EXPECT_THROW(builder.build(std::numeric_limits<size_t>::max(), 2), std::runtime_error);
}

TEST(WBEROBufferVKBuilderTest, InitialDataOwnsSourceBytes) {
    std::array<uint8_t, 4> data{1, 2, 3, 4};
    ROBufferVK::Builder builder;

    builder.initial_data(std::span<const uint8_t>(data));
    data[0] = 9;

    EXPECT_EQ(builder.size, data.size());
    EXPECT_EQ(WBE_REQUIRED(builder.domain), DeviceDomain::HOST_DEVICE);
    EXPECT_EQ(builder.initial_data_bytes, (std::vector<uint8_t>{1, 2, 3, 4}));
}

TEST(WBEROBufferVKBuilderTest, DeviceInitialDataAddsTransferDestinationUsage) {
    const std::array<uint8_t, 2> data{5, 6};
    ROBufferVK::Builder builder;

    builder.initial_data(std::span<const uint8_t>(data), DeviceDomain::DEVICE);

    ASSERT_TRUE(builder.usage.has_value());
    EXPECT_NE(WBE_REQUIRED(builder.usage) & VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0U);
}

TEST(WBEROBufferVKBuilderTest, InitialDataRejectsEmptySpan) {
    ROBufferVK::Builder builder;

    EXPECT_THROW(builder.initial_data(std::span<const uint8_t>()), std::runtime_error);
}

} // namespace WhiteBirdEngine
