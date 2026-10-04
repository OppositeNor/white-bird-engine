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
#ifndef WBE_FILE_RENDER_VK_DEFS_HH
#define WBE_FILE_RENDER_VK_DEFS_HH

#include "function/render/i_renderer.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/ktx_vulkan_device_info_vk.hh"
#include "function/render_vk/vma_vk.hh"

#include <cstdint>
#include <functional>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class DescriptorHeapAllocatorVK;

/** @brief Identifies a physical Vulkan descriptor heap owned by the renderer. */
using DescriptorHeapIDVK = uint32_t;
/** @brief Identifies an arena within a physical Vulkan descriptor heap. */
using DescriptorArenaIDVK = uint32_t;

/**
 * @struct DescriptorArenaLocationVK
 * @brief Identifies an arena in a physical descriptor heap.
 */
struct DescriptorArenaLocationVK {
    /** Physical descriptor heap identifier. */
    DescriptorHeapIDVK heap_id = UINT32_MAX;
    /** Arena identifier local to that heap. */
    DescriptorArenaIDVK arena_id = UINT32_MAX;
};

/** One-shot Vulkan command-buffer recording job. */
using GPUJobVK = std::function<void(VkCommandBuffer)>;

/**
 * @class BaseRayTracingDataVK
 * @brief Vulkan procedures, limits, and shared handles used by ray-tracing render objects.
 */
struct BaseRayTracingDataVK {
    /**
     * @brief Device-reported size of an acceleration-structure descriptor in the resource heap.
     */
    VkDeviceSize acceleration_structure_descriptor_size = 0;
    /**
     * @brief Arena containing acceleration-structure descriptors.
     */
    DescriptorArenaLocationVK acceleration_structure_descriptor_arena{};
    /**
     * @brief Device command for creating an acceleration structure.
     */
    PFN_vkCreateAccelerationStructureKHR create_acceleration_structure = nullptr;
    /**
     * @brief Device command for destroying an acceleration structure.
     */
    PFN_vkDestroyAccelerationStructureKHR destroy_acceleration_structure = nullptr;
    /**
     * @brief Device command for querying acceleration-structure build sizes.
     */
    PFN_vkGetAccelerationStructureBuildSizesKHR get_acceleration_structure_build_sizes = nullptr;
    /**
     * @brief Device command for recording acceleration-structure builds.
     */
    PFN_vkCmdBuildAccelerationStructuresKHR command_build_acceleration_structures = nullptr;
    /**
     * @brief Device command for querying an acceleration-structure device address.
     */
    PFN_vkGetAccelerationStructureDeviceAddressKHR get_acceleration_structure_device_address = nullptr;
    /**
     * @brief Required device-address alignment for acceleration-structure scratch storage.
     */
    VkDeviceSize min_acceleration_structure_scratch_offset_alignment = 1;
    /**
     * @brief Maximum geometries accepted by one acceleration structure.
     */
    uint64_t max_acceleration_structure_geometry_count = 0;
    /**
     * @brief Maximum primitives accepted by one acceleration structure.
     */
    uint64_t max_acceleration_structure_primitive_count = 0;
};

/**
 * @class BaseRenderDataVK
 * @brief The base render data that is commonly used throughout
 * the render objects.
 *
 */
struct BaseRenderDataVK {
    BaseRenderDataVK(RenderAllocator* p_allocator) : allocator(p_allocator), queue_family_indices(p_allocator) {
    }
    // The render allocator.
    RenderAllocator* allocator = nullptr;
    // The render database.
    RenderDatabase* render_database = nullptr;
    /** Capabilities negotiated when the renderer was constructed. */
    RendererCapabilities renderer_capabilities{};

    // The logic device.
    VkDevice logic_device = VK_NULL_HANDLE;
    // The physical device that owns the renderer queues and resources.
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    /** Command for setting dynamic vertex input state. */
    PFN_vkCmdSetVertexInputEXT command_set_vertex_input = nullptr;
    /** The VMA allocator used for owned Vulkan buffers and images. */
    VmaAllocator vma_allocator = VK_NULL_HANDLE;
    /** The image format of the swap chain. */
    VkFormat swap_chain_format = VK_FORMAT_UNDEFINED;
    /** The size of the swap chain. */
    VkExtent2D swap_chain_extent{};
    /** The aspect ratio of the swap chain. */
    float swap_chain_aspect_ratio = 1.0F;
    /** Callback for allocator. */
    VkAllocationCallbacks* allocation_callbacks = nullptr;
    /** Graphics queue family. */
    VkQueue graphics_queue = VK_NULL_HANDLE;
    /** Compute queue family. */
    VkQueue compute_queue = VK_NULL_HANDLE;
    /** The viewport for the final presentation. */
    VkViewport viewport{};
    /** The scissor for the final presentation. */
    VkRect2D scissor{};

    /** Graphics command pool. */
    VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
    /** Compute command pool. */
    VkCommandPool compute_command_pool = VK_NULL_HANDLE;
    /** Non-owning libktx Vulkan upload context owned by RendererVK. */
    KTXVulkanDeviceInfoVK* ktx_vulkan_device_info = nullptr;
    /** Descriptor-heap properties reported by the physical device. */
    VkPhysicalDeviceDescriptorHeapPropertiesEXT descriptor_heap_properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT};
    /** Renderer-owned resource descriptor heap. */
    DescriptorHeapIDVK resource_descriptor_heap_id = UINT32_MAX;
    /** Renderer-owned sampler descriptor heap. */
    DescriptorHeapIDVK sampler_descriptor_heap_id = UINT32_MAX;
    /** Renderer-owned buffer descriptor arena. */
    DescriptorArenaLocationVK buffer_descriptor_arena{};
    /** Renderer-owned sampled-image descriptor arena. */
    DescriptorArenaLocationVK sampled_image_descriptor_arena{};
    /** Number of slots in the sampled-image descriptor arena. */
    uint32_t sampled_image_descriptor_capacity = 0;
    /** Renderer-owned storage-image descriptor arena. */
    DescriptorArenaLocationVK storage_image_descriptor_arena{};
    /** Renderer-owned sampler descriptor arena. */
    DescriptorArenaLocationVK sampler_descriptor_arena{};
    /** Stride used by sampler descriptors. */
    VkDeviceSize sampler_descriptor_stride = 0;
    /** Non-owning renderer descriptor-heap allocator. */
    DescriptorHeapAllocatorVK* descriptor_heap_allocator = nullptr;
    /** Host command for encoding resource descriptors. */
    PFN_vkWriteResourceDescriptorsEXT write_resource_descriptors = nullptr;
    /** Host command for encoding sampler descriptors. */
    PFN_vkWriteSamplerDescriptorsEXT write_sampler_descriptors = nullptr;
    /** Command for binding the resource descriptor heap. */
    PFN_vkCmdBindResourceHeapEXT command_bind_resource_heap = nullptr;
    /** Command for binding the sampler descriptor heap. */
    PFN_vkCmdBindSamplerHeapEXT command_bind_sampler_heap = nullptr;
    /** Command for uploading descriptor indices and other pipeline push data. */
    PFN_vkCmdPushDataEXT command_push_data = nullptr;
    /** Maximum indexed-indirect draws accepted by one Vulkan draw call. */
    uint32_t max_draw_indirect_count = 1;
    /** The fence that blocks every render tick. */
    VkFence in_flight_fence = VK_NULL_HANDLE;
    /** The current swap chain index. This is useful to trach which frame in
     * flight it is right now. */
    uint32_t current_swap_chain_index = 0;
    /** The number of images owned by the current swap chain. */
    uint32_t swap_chain_images_count = 0;
    /** The current frame-in-flight slot (matches `in_flight_fence` slot index). */
    uint32_t current_frame_slot = 0;
    /** Frames in flight. */
    uint32_t frames_in_flight = 0;

    /** The current number of frames. */
    uint64_t current_frame = 0;
    /** Elapsed renderer update time in seconds. */
    double time = 0.0;

    /** The queue family indices. */
    Set<uint32_t, RenderAllocator> queue_family_indices;

    /** Function to register a delayed call. */
    std::function<void(std::function<void()>&& p_function, uint32_t p_delayed_frames)> register_delayed_call;
};

/**
 * @brief The bindpoint for vertex buffer.
 */
constexpr uint32_t VERTEX_BUFFER_BINDPOINT = 0;

} // namespace WhiteBirdEngine

#endif
