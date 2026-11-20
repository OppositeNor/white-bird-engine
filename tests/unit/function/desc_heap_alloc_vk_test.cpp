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
#include "function/render/rend_defs.hh"
#include "function/rend_vk/desc_heap_alloc_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_desc_heap_vk.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBEDescHeapConfigVKTest, ReservesConsecutiveArenasAlignedToTheirStride) {
    DescHeapConfigVK config;

    EXPECT_EQ(config.reserve_arena_bytes(3, 16, 16), 0U);
    EXPECT_EQ(config.usable_size, 48U);
    // 48 is not a multiple of 32, so the second arena starts at the next multiple.
    EXPECT_EQ(config.reserve_arena_bytes(2, 32, 16), 64U);
    EXPECT_EQ(config.usable_size, 128U);
    EXPECT_EQ(config.reserve_arena_bytes(1, 8, 8), 128U);
    EXPECT_EQ(config.usable_size, 136U);
}

TEST(WBEDescHeapConfigVKTest, ReserveRejectsZeroCount) {
    DescHeapConfigVK config;

    EXPECT_THROW((void)config.reserve_arena_bytes(0, 16, 16), std::runtime_error);
}

TEST(WBEDescHeapConfigVKTest, ReserveRejectsZeroStride) {
    DescHeapConfigVK config;

    EXPECT_THROW((void)config.reserve_arena_bytes(1, 0, 16), std::runtime_error);
}

TEST(WBEDescHeapConfigVKTest, ReserveRejectsZeroAlignment) {
    DescHeapConfigVK config;

    EXPECT_THROW((void)config.reserve_arena_bytes(1, 16, 0), std::runtime_error);
}

TEST(WBEDescHeapConfigVKTest, ReserveRejectsStrideNotMultipleOfAlignment) {
    DescHeapConfigVK config;

    EXPECT_THROW((void)config.reserve_arena_bytes(1, 24, 16), std::runtime_error);
}

TEST(WBEDescHeapConfigVKTest, ReserveRejectsSizeOverflow) {
    DescHeapConfigVK config;
    config.usable_size = std::numeric_limits<VkDeviceSize>::max() - 8;

    EXPECT_THROW((void)config.reserve_arena_bytes(1, 16, 16), std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, RejectsMissingAlloc) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);

    EXPECT_THROW(DescHeapAllocVK(nullptr, &rend_data), std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, RejectsMissingRendData) {
    RendAlloc alloc(WBE_KI_B(64));

    EXPECT_THROW(DescHeapAllocVK(&alloc, nullptr), std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, CreateHeapRejectsZeroUsableSize) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.desc_heap_properties.maxResourceHeapSize = WBE_KI_B(4);
    DescHeapAllocVK heap_alloc(&alloc, &rend_data);

    EXPECT_THROW((void)heap_alloc.create_heap({.type = DescHeapTypeVK::RESOURCE, .usable_size = 0, .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, CreateHeapRejectsZeroAlignment) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.desc_heap_properties.maxResourceHeapSize = WBE_KI_B(4);
    DescHeapAllocVK heap_alloc(&alloc, &rend_data);

    EXPECT_THROW((void)heap_alloc.create_heap({.type = DescHeapTypeVK::RESOURCE, .usable_size = 64, .alignment = 0}),
        std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, CreateHeapRejectsResourceHeapAboveDeviceLimit) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.desc_heap_properties.maxResourceHeapSize = 128;
    // The sampler limit does not apply to resource heaps.
    rend_data.desc_heap_properties.maxSamplerHeapSize = WBE_KI_B(4);
    DescHeapAllocVK heap_alloc(&alloc, &rend_data);

    // The reserved range counts toward the limit.
    EXPECT_THROW((void)heap_alloc.create_heap(
                     {.type = DescHeapTypeVK::RESOURCE, .usable_size = 96, .reserved_size = 64, .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, CreateHeapRejectsSamplerHeapAboveDeviceLimit) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.desc_heap_properties.maxResourceHeapSize = WBE_KI_B(4);
    rend_data.desc_heap_properties.maxSamplerHeapSize = 128;
    DescHeapAllocVK heap_alloc(&alloc, &rend_data);

    EXPECT_THROW((void)heap_alloc.create_heap({.type = DescHeapTypeVK::SAMPLER, .usable_size = 256, .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, CreateHeapRejectsReservedSizeOverflow) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    rend_data.desc_heap_properties.maxResourceHeapSize = std::numeric_limits<VkDeviceSize>::max();
    DescHeapAllocVK heap_alloc(&alloc, &rend_data);

    EXPECT_THROW((void)heap_alloc.create_heap({.type = DescHeapTypeVK::RESOURCE,
                     .usable_size = 64,
                     .reserved_size = std::numeric_limits<VkDeviceSize>::max(),
                     .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescHeapAllocVKTest, OperationsRejectUnknownHeap) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    DescHeapAllocVK heap_alloc(&alloc, &rend_data);
    constexpr DescHeapIDVK HEAP_ID = 0;
    constexpr DescArenaIDVK ARENA_ID = 0;
    const DescArenaConfigVK arena_config{
        .byte_offset = 0,
        .desc_stride = 16,
        .desc_count = 4,
        .desc_alignment = 16,
    };

    EXPECT_THROW((void)heap_alloc.allocate_arena(HEAP_ID, arena_config), std::runtime_error);
    EXPECT_THROW((void)heap_alloc.allocate_desc(HEAP_ID, ARENA_ID), std::runtime_error);
    EXPECT_THROW(heap_alloc.deallocate_desc(HEAP_ID, ARENA_ID, 0), std::runtime_error);
    EXPECT_THROW((void)heap_alloc.owns_desc(HEAP_ID, ARENA_ID, 0), std::runtime_error);
    EXPECT_THROW(heap_alloc.write_resource_desc(HEAP_ID, ARENA_ID, 0, VkResourceDescriptorInfoEXT{}),
        std::runtime_error);
    EXPECT_THROW(heap_alloc.write_sampler_desc(HEAP_ID, ARENA_ID, 0, VkSamplerCreateInfo{}), std::runtime_error);
    EXPECT_THROW(heap_alloc.bind_heaps(VK_NULL_HANDLE, HEAP_ID, HEAP_ID), std::runtime_error);
}

} // namespace WhiteBirdEngine
