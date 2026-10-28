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
#include "function/render/render_defs.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/ro_descriptor_heap_vk.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBEDescriptorHeapConfigurationVKTest, ReservesConsecutiveArenasAlignedToTheirStride) {
    DescriptorHeapConfigurationVK configuration;

    EXPECT_EQ(configuration.reserve_arena_bytes(3, 16, 16), 0U);
    EXPECT_EQ(configuration.usable_size, 48U);
    // 48 is not a multiple of 32, so the second arena starts at the next multiple.
    EXPECT_EQ(configuration.reserve_arena_bytes(2, 32, 16), 64U);
    EXPECT_EQ(configuration.usable_size, 128U);
    EXPECT_EQ(configuration.reserve_arena_bytes(1, 8, 8), 128U);
    EXPECT_EQ(configuration.usable_size, 136U);
}

TEST(WBEDescriptorHeapConfigurationVKTest, ReserveRejectsZeroCount) {
    DescriptorHeapConfigurationVK configuration;

    EXPECT_THROW((void)configuration.reserve_arena_bytes(0, 16, 16), std::runtime_error);
}

TEST(WBEDescriptorHeapConfigurationVKTest, ReserveRejectsZeroStride) {
    DescriptorHeapConfigurationVK configuration;

    EXPECT_THROW((void)configuration.reserve_arena_bytes(1, 0, 16), std::runtime_error);
}

TEST(WBEDescriptorHeapConfigurationVKTest, ReserveRejectsZeroAlignment) {
    DescriptorHeapConfigurationVK configuration;

    EXPECT_THROW((void)configuration.reserve_arena_bytes(1, 16, 0), std::runtime_error);
}

TEST(WBEDescriptorHeapConfigurationVKTest, ReserveRejectsStrideNotMultipleOfAlignment) {
    DescriptorHeapConfigurationVK configuration;

    EXPECT_THROW((void)configuration.reserve_arena_bytes(1, 24, 16), std::runtime_error);
}

TEST(WBEDescriptorHeapConfigurationVKTest, ReserveRejectsSizeOverflow) {
    DescriptorHeapConfigurationVK configuration;
    configuration.usable_size = std::numeric_limits<VkDeviceSize>::max() - 8;

    EXPECT_THROW((void)configuration.reserve_arena_bytes(1, 16, 16), std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, RejectsMissingAllocator) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);

    EXPECT_THROW(DescriptorHeapAllocatorVK(nullptr, &render_data), std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, RejectsMissingRenderData) {
    RenderAllocator allocator(WBE_KI_B(64));

    EXPECT_THROW(DescriptorHeapAllocatorVK(&allocator, nullptr), std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, CreateHeapRejectsZeroUsableSize) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);
    render_data.descriptor_heap_properties.maxResourceHeapSize = WBE_KI_B(4);
    DescriptorHeapAllocatorVK heap_allocator(&allocator, &render_data);

    EXPECT_THROW((void)heap_allocator.create_heap({.type = DescriptorHeapTypeVK::RESOURCE, .usable_size = 0, .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, CreateHeapRejectsZeroAlignment) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);
    render_data.descriptor_heap_properties.maxResourceHeapSize = WBE_KI_B(4);
    DescriptorHeapAllocatorVK heap_allocator(&allocator, &render_data);

    EXPECT_THROW((void)heap_allocator.create_heap({.type = DescriptorHeapTypeVK::RESOURCE, .usable_size = 64, .alignment = 0}),
        std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, CreateHeapRejectsResourceHeapAboveDeviceLimit) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);
    render_data.descriptor_heap_properties.maxResourceHeapSize = 128;
    // The sampler limit does not apply to resource heaps.
    render_data.descriptor_heap_properties.maxSamplerHeapSize = WBE_KI_B(4);
    DescriptorHeapAllocatorVK heap_allocator(&allocator, &render_data);

    // The reserved range counts toward the limit.
    EXPECT_THROW((void)heap_allocator.create_heap(
                     {.type = DescriptorHeapTypeVK::RESOURCE, .usable_size = 96, .reserved_size = 64, .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, CreateHeapRejectsSamplerHeapAboveDeviceLimit) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);
    render_data.descriptor_heap_properties.maxResourceHeapSize = WBE_KI_B(4);
    render_data.descriptor_heap_properties.maxSamplerHeapSize = 128;
    DescriptorHeapAllocatorVK heap_allocator(&allocator, &render_data);

    EXPECT_THROW((void)heap_allocator.create_heap({.type = DescriptorHeapTypeVK::SAMPLER, .usable_size = 256, .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, CreateHeapRejectsReservedSizeOverflow) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);
    render_data.descriptor_heap_properties.maxResourceHeapSize = std::numeric_limits<VkDeviceSize>::max();
    DescriptorHeapAllocatorVK heap_allocator(&allocator, &render_data);

    EXPECT_THROW((void)heap_allocator.create_heap({.type = DescriptorHeapTypeVK::RESOURCE,
                     .usable_size = 64,
                     .reserved_size = std::numeric_limits<VkDeviceSize>::max(),
                     .alignment = 16}),
        std::runtime_error);
}

TEST(WBEDescriptorHeapAllocatorVKTest, OperationsRejectUnknownHeap) {
    RenderAllocator allocator(WBE_KI_B(64));
    RenderContextVK render_data(&allocator);
    DescriptorHeapAllocatorVK heap_allocator(&allocator, &render_data);
    constexpr DescriptorHeapIDVK HEAP_ID = 0;
    constexpr DescriptorArenaIDVK ARENA_ID = 0;
    const DescriptorArenaConfigurationVK arena_configuration{
        .byte_offset = 0,
        .descriptor_stride = 16,
        .descriptor_count = 4,
        .descriptor_alignment = 16,
    };

    EXPECT_THROW((void)heap_allocator.allocate_arena(HEAP_ID, arena_configuration), std::runtime_error);
    EXPECT_THROW((void)heap_allocator.allocate_descriptor(HEAP_ID, ARENA_ID), std::runtime_error);
    EXPECT_THROW(heap_allocator.deallocate_descriptor(HEAP_ID, ARENA_ID, 0), std::runtime_error);
    EXPECT_THROW((void)heap_allocator.owns_descriptor(HEAP_ID, ARENA_ID, 0), std::runtime_error);
    EXPECT_THROW(heap_allocator.write_resource_descriptor(HEAP_ID, ARENA_ID, 0, VkResourceDescriptorInfoEXT{}),
        std::runtime_error);
    EXPECT_THROW(heap_allocator.write_sampler_descriptor(HEAP_ID, ARENA_ID, 0, VkSamplerCreateInfo{}), std::runtime_error);
    EXPECT_THROW(heap_allocator.bind_heaps(VK_NULL_HANDLE, HEAP_ID, HEAP_ID), std::runtime_error);
}

} // namespace WhiteBirdEngine
