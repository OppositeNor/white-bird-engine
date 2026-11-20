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
#include "function/rend_vk/desc_set_plugin_vk.hh"
#include "function/render/i_rend_task.hh"
#include "function/render/i_renderer.hh"
#include "function/rend_vk/desc_heap_alloc_vk.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "utils/defs.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

constexpr std::array<VkDescriptorType, 7> POOL_DESC_TYPES{
    VK_DESCRIPTOR_TYPE_SAMPLER,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
};

} // namespace

DescSetPluginVK::DescSetPluginVK(const RendContextVK* p_rend_data) : rend_data(p_rend_data) {
    singleton = this;
}

DescSetPluginVK::~DescSetPluginVK() {
    // The pool is released by on_renderer_terminating(); this only covers renderers that failed before termination.
    destroy_desc_pool();
    singleton = nullptr;
}

VkInstance DescSetPluginVK::get_instance() const {
    if (vk_instance == VK_NULL_HANDLE) {
        throw std::runtime_error("DescSetPluginVK: the Vulkan instance has not been created.");
    }
    return vk_instance;
}

VkDescriptorPool DescSetPluginVK::get_desc_pool() const {
    if (desc_pool == VK_NULL_HANDLE) {
        throw std::runtime_error("DescSetPluginVK: the descriptor pool is unavailable.");
    }
    return desc_pool;
}

VkDescriptorSet DescSetPluginVK::allocate_desc_set(VkDescriptorSetLayout p_layout) {
    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = get_desc_pool();
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &p_layout;
    VkDescriptorSet result = VK_NULL_HANDLE;
    VkResult allocate_result = vkAllocateDescriptorSets(logical_device, &allocate_info, &result);
    if (allocate_result != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("DescSetPluginVK: failed to allocate a descriptor set: {}.", string_VkResult(allocate_result)));
    }
    return result;
}

void DescSetPluginVK::free_desc_set(VkDescriptorSet p_desc_set) {
    VkResult free_result = vkFreeDescriptorSets(logical_device, get_desc_pool(), 1, &p_desc_set);
    if (free_result != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("DescSetPluginVK: failed to free a descriptor set: {}.", string_VkResult(free_result)));
    }
}

bool DescSetPluginVK::try_enable(
    RendererCapabilities& p_capabilities, VkPhysicalDevice /*p_physical_device*/, const char* const* /*p_enabled_layers*/, size_t /*p_enabled_layer_count*/) {
    enabled = p_capabilities.desc_set;
    return enabled;
}

std::span<const char* const> DescSetPluginVK::get_required_instance_extensions() const {
    return {};
}

std::span<const char* const> DescSetPluginVK::get_required_device_extensions() const {
    // Descriptor sets are core Vulkan.
    return {};
}

void DescSetPluginVK::on_before_instance_creation(VkInstanceCreateInfo* /*p_create_info*/) {
}

void DescSetPluginVK::on_after_instance_created(VkInstance p_instance) {
    vk_instance = p_instance;
}

void DescSetPluginVK::on_before_device_creation(
    VkPhysicalDevice /*p_physical_device*/, VkDeviceCreateInfo* /*p_create_info*/, VkPhysicalDeviceFeatures* /*p_device_features*/) {
}

void DescSetPluginVK::on_after_device_created(
    RendContextVK* /*p_rend_data*/, VkPhysicalDevice /*p_physical_device*/, VkDevice p_logical_device) {
    WBE_DEBUG_ASSERT(enabled);
    WBE_DEBUG_ASSERT(desc_pool == VK_NULL_HANDLE);
    logical_device = p_logical_device;
    std::array<VkDescriptorPoolSize, POOL_DESC_TYPES.size()> pool_sizes{};
    for (size_t i = 0; i < POOL_DESC_TYPES.size(); ++i) {
        pool_sizes[i] = VkDescriptorPoolSize{.type = POOL_DESC_TYPES[i], .descriptorCount = DESCS_PER_TYPE};
    }
    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_create_info.maxSets = MAX_DESC_SETS;
    pool_create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_create_info.pPoolSizes = pool_sizes.data();
    VkResult create_result =
        vkCreateDescriptorPool(logical_device, &pool_create_info, rend_data->allocation_callbacks, &desc_pool);
    if (create_result != VK_SUCCESS) {
        desc_pool = VK_NULL_HANDLE;
        throw std::runtime_error(
            std::format("DescSetPluginVK: failed to create the descriptor pool: {}.", string_VkResult(create_result)));
    }
}

void DescSetPluginVK::update_rend_context(RendContextVK* /*p_rend_data*/, VkPhysicalDevice /*p_physical_device*/) {
}

void DescSetPluginVK::configure_desc_heaps(
    const RendContextVK* /*p_rend_data*/, std::span<DescHeapConfigVK> /*p_configs*/) {
}

void DescSetPluginVK::create_desc_arenas(RendContextVK* /*p_rend_data*/) {
}

void DescSetPluginVK::on_frame_begin(const RendContextVK* /*p_rend_data*/) {
}

void DescSetPluginVK::on_rend_graph_registered(std::string_view /*p_graph_key*/, const RendGraphVK& /*p_graph*/) {
}

void DescSetPluginVK::on_before_task_recorded(VkCommandBuffer /*p_command_buffer*/,
    const RendGraphVK& /*p_graph*/,
    const RendTaskID& /*p_task_id*/,
    QueueFamilyVK /*p_queue_family*/) {
}

void DescSetPluginVK::on_after_task_recorded(VkCommandBuffer /*p_command_buffer*/,
    const RendGraphVK& /*p_graph*/,
    const RendTaskID& /*p_task_id*/,
    QueueFamilyVK /*p_queue_family*/) {
}

void DescSetPluginVK::on_renderer_terminating() {
    destroy_desc_pool();
}

void DescSetPluginVK::destroy_desc_pool() {
    if (desc_pool == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyDescriptorPool(logical_device, desc_pool, rend_data->allocation_callbacks);
    desc_pool = VK_NULL_HANDLE;
}

} // namespace WhiteBirdEngine
