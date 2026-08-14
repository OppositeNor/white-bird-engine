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

#include "core/surface/3d_primitive.hh"
#include "function/function_utils.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/vma_vk.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class BaseRenderDataVK
 * @brief The base render data that is commonly used throughout
 * the render objects.
 *
 */
struct BaseRenderDataVK {
    BaseRenderDataVK(RenderAllocator* p_allocator)
        : queue_family_indices(p_allocator), pre_execute_tasks_queue(p_allocator) {
    }
    // The render allocator.
    RenderAllocator* allocator = nullptr;
    // The render database.
    RenderDatabase* render_database = nullptr;

    // The logic device.
    VkDevice logic_device = VK_NULL_HANDLE;
    // The VMA allocator used for owned Vulkan buffers and images.
    VmaAllocator vma_allocator = VK_NULL_HANDLE;
    // The image format of the swap chain.
    VkFormat swap_chain_format = VK_FORMAT_UNDEFINED;
    // The size of the swap chain.
    VkExtent2D swap_chain_extent{};
    // The aspect ratio of the swap chain.
    float swap_chain_aspect_ratio = 1.0F;
    // Callback for allocator.
    VkAllocationCallbacks* allocation_callbacks = nullptr;
    // Graphics queue family.
    VkQueue graphics_queue = VK_NULL_HANDLE;
    // Compute queue family.
    VkQueue compute_queue = VK_NULL_HANDLE;
    // The viewport for the final presentation.
    VkViewport viewport{};
    // The scissor for the final presentation.
    VkRect2D scissor{};

    // Graphics command pool.
    VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
    // Compute command pool.
    VkCommandPool compute_command_pool = VK_NULL_HANDLE;
    // Descriptor pool.
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    /**
     * @brief Maximum sampled-image descriptors allowed for a variable-count binding.
     *
     * This is capped by both the renderer descriptor pool and the physical-device descriptor limits.
     */
    uint32_t max_variable_sampled_image_descriptor_count = 1;

    // The fence that blocks every render tick.
    VkFence in_flight_fence = VK_NULL_HANDLE;
    // The current swap chain index. This is useful to trach which frame in
    // flight it is right now.
    uint32_t current_swap_chain_index = 0;
    // The number of images owned by the current swap chain.
    uint32_t swap_chain_images_count = 0;
    // The current frame-in-flight slot (matches `in_flight_fence` slot index).
    uint32_t current_frame_slot = 0;
    // Frames in flight.
    uint32_t frames_in_flight = 0;

    // The current number of frames.
    uint64_t current_frame = 0;
    // The maximum depth of the render graph in the current pipeline.
    uint64_t max_depth = 0;

    // The queue family indices.
    Set<uint32_t, RenderAllocator> queue_family_indices;

    // Task to execute in the pre-execution task.
    Vector<std::function<void(VkCommandBuffer)>, RenderAllocator> pre_execute_tasks_queue;

    // Function to register a delayed call.
    std::function<void(std::function<void()>&& p_function, uint32_t p_delayed_frames)> register_delayed_call;
};

/**
 * @brief The bindpoint for vertex buffer.
 */
constexpr uint32_t VERTEX_BUFFER_BINDPOINT = 0;

/**
 * @brief The binding description for Vertex3DTextured.
 */
constexpr VkVertexInputBindingDescription VERTEX_3D_TEXTURED_INPUT_BINDING_DESCRIPTION = {
    .binding = VERTEX_BUFFER_BINDPOINT, .stride = sizeof(Vertex3DTextured), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

/**
 * @brief The vertex input attribute description of Vertex3DTextured.
 */
constexpr std::array<VkVertexInputAttributeDescription, 5> VERTEX_3D_TEXTURED_INPUT_ATTRIBUTE_DESCRIPTION = {
    VkVertexInputAttributeDescription{// position
        .location = 0,
        .binding = VERTEX_BUFFER_BINDPOINT,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex3DTextured, position)},
    {// normal
        .location = 1,
        .binding = VERTEX_BUFFER_BINDPOINT,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex3DTextured, normal)},
    {// tangent
        .location = 2,
        .binding = VERTEX_BUFFER_BINDPOINT,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex3DTextured, tangent)},
    {// bitangent
        .location = 3,
        .binding = VERTEX_BUFFER_BINDPOINT,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex3DTextured, bitangent)},
    {// uv
        .location = 4,
        .binding = VERTEX_BUFFER_BINDPOINT,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex3DTextured, uv)}};

struct CameraInfoVKUBO {
    /**
     * @brief The view matrix.
     */
    glm::mat4 view;
    /**
     * @brief The projection matrix.
     */
    glm::mat4 proj;
};

constexpr uint32_t MAX_SPOT_LIGHT_INFO_COUNT = 4;

struct SpotLightInfoVKUBO {
    /**
     * @brief Light-space view matrix.
     */
    glm::mat4 view;
    /**
     * @brief Light-space projection matrix.
     */
    glm::mat4 proj;
    /**
     * @brief World-space position of the spot light.
     */
    glm::vec3 position;
    /**
     * @brief Scalar intensity multiplier.
     */
    float intensity = 0.0F;
    /**
     * @brief World-space direction the spot is pointing.
     */
    glm::vec3 direction;
    /**
     * @brief Cosine of the inner cone half-angle.
     */
    float inner_cone_cos = 0.0F;
    /**
     * @brief Linear-space color of the spot light.
     */
    glm::vec3 color;
    /**
     * @brief Cosine of the outer cone half-angle.
     */
    float outer_cone_cos = 0.0F;
    /**
     * @brief Maximum effective light distance.
     */
    float range = 0.0F;
    /**
     * @brief Padding to match the shader struct array stride.
     */
    std::array<uint32_t, 3> padding{};
};

static_assert(sizeof(SpotLightInfoVKUBO) == 192);

struct SpotLightInfosVKUBO {
    /**
     * @brief Active spot light data consumed by the shader.
     */
    std::array<SpotLightInfoVKUBO, MAX_SPOT_LIGHT_INFO_COUNT> spot_light_info{};
    /**
     * @brief Number of active entries in spot_light_info.
     */
    uint32_t spot_light_count = 0;
};

/**
 * @brief Time information.
 */
struct TimeVKUBO {
    /**
     * @brief The duration from the start of the application until now.
     */
    float duration_sec = 0.0F;
};

/**
 * @brief The maximum number of descriptors per descriptor set.
 */
constexpr uint32_t MAX_DESCRIPTORS_PER_SET = 16;

} // namespace WhiteBirdEngine

#endif
