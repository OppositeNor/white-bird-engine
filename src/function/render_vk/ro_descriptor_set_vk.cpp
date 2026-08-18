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
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {
using BoundObjectReferencesVK = Vector<Ref<RenderObjectVK>, RenderAllocator>;
using PendingBoundObjectsVK = Vector<BoundObjectReferencesVK, RenderAllocator>;

uint64_t descriptor_array_element_key(uint32_t p_binding, uint32_t p_array_element) {
    return (static_cast<uint64_t>(p_binding) << 32U) | p_array_element;
}

void write_buffer_info(VkDescriptorType p_descriptor_type,
    VkWriteDescriptorSet& p_write_info,
    const std::vector<RODescriptorSetVK::Writer::BufferInfo>& p_buffer_infos,
    RenderAllocator* p_allocator,
    PendingBoundObjectsVK& p_pending_bound_objects,
    std::vector<VkDescriptorBufferInfo>& p_result_buffer_infos) {
    WBE_DEBUG_ASSERT(p_result_buffer_infos.empty());
    WBE_DEBUG_ASSERT(p_pending_bound_objects.empty());
    p_write_info.descriptorType = p_descriptor_type;
    p_result_buffer_infos.reserve(p_buffer_infos.size());
    p_pending_bound_objects.reserve(p_buffer_infos.size());
    for (const auto& buffer_info : p_buffer_infos) {
        VkDescriptorBufferInfo vk_buffer_info{};
        vk_buffer_info.buffer = buffer_info.buffer->get_buffer();
        vk_buffer_info.offset = buffer_info.offset;
        vk_buffer_info.range = buffer_info.range;
        p_result_buffer_infos.push_back(vk_buffer_info);
        // Keep a reference to the buffer to prevent it from being destroyed.
        BoundObjectReferencesVK pending_bound_objects(p_allocator);
        pending_bound_objects.emplace_back(buffer_info.buffer);
        p_pending_bound_objects.push_back(std::move(pending_bound_objects));
    }
    p_write_info.descriptorCount = p_result_buffer_infos.size();
    p_write_info.pBufferInfo = p_result_buffer_infos.data();
}

void write_image_info(VkDescriptorType p_descriptor_type,
    VkWriteDescriptorSet& p_write_info,
    const std::vector<RODescriptorSetVK::Writer::ImageInfo>& p_image_infos,
    RenderAllocator* p_allocator,
    PendingBoundObjectsVK& p_pending_bound_objects,
    std::vector<VkDescriptorImageInfo>& p_result_image_infos) {
    WBE_DEBUG_ASSERT(p_result_image_infos.empty());
    WBE_DEBUG_ASSERT(p_pending_bound_objects.empty());
    p_write_info.descriptorType = p_descriptor_type;
    p_result_image_infos.reserve(p_image_infos.size());
    p_pending_bound_objects.reserve(p_image_infos.size());
    for (const auto& image_info : p_image_infos) {
        VkDescriptorImageInfo vk_image_info{};
        BoundObjectReferencesVK pending_bound_objects(p_allocator);
        pending_bound_objects.reserve(2);
        vk_image_info.imageLayout = image_info.image_layout;
        if (p_descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
            if (image_info.image_view == MEM_NULL) {
                throw std::runtime_error("Failed to write descriptor set: image view is required.");
            }
            vk_image_info.imageView = image_info.image_view->get_image_view();
            // Keep the image view alive for as long as this descriptor set uses it.
            pending_bound_objects.emplace_back(image_info.image_view);
        }
        if (p_descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
            if (image_info.sampler == MEM_NULL) {
                throw std::runtime_error("Failed to write descriptor set: sampler is required.");
            }
            vk_image_info.sampler = image_info.sampler->get_sampler();
            // Keep the sampler alive for as long as this descriptor set uses it.
            pending_bound_objects.emplace_back(image_info.sampler);
        }
        p_result_image_infos.push_back(vk_image_info);
        p_pending_bound_objects.push_back(std::move(pending_bound_objects));
    }
    p_write_info.descriptorCount = p_result_image_infos.size();
    p_write_info.pImageInfo = p_result_image_infos.data();
}

} // namespace

VkDescriptorSet RODescriptorSetVK::Builder::create_vk_object(const BaseRenderDataVK* p_render_data) const {
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = p_render_data->descriptor_pool;
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.reserve(descriptor_set_layouts.size());
    for (const auto& layout : descriptor_set_layouts) {
        layouts.push_back(layout->get_descriptor_set_layout());
    }
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();
    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info{};
    if (variable_descriptor_count != 0) {
        if (layouts.size() != 1) {
            throw std::runtime_error("Failed to allocate variable-count descriptor set: exactly one layout is required.");
        }
        uint32_t variable_descriptor_count_upper_bound = descriptor_set_layouts.front()->get_variable_descriptor_count_upper_bound();
        if (variable_descriptor_count_upper_bound == 0 || variable_descriptor_count > variable_descriptor_count_upper_bound) {
            throw std::runtime_error(std::format("Failed to allocate variable-count descriptor set: requested count {} exceeds layout upper bound {}.",
                variable_descriptor_count,
                variable_descriptor_count_upper_bound));
        }
        variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variable_count_info.descriptorSetCount = 1;
        variable_count_info.pDescriptorCounts = &variable_descriptor_count;
        alloc_info.pNext = &variable_count_info;
    }
    VkDescriptorSet result_descriptor_set;
    VkResult allocate_result = vkAllocateDescriptorSets(p_render_data->logic_device, &alloc_info, &result_descriptor_set);
    if (allocate_result) {
        throw std::runtime_error(std::format("Failed to allocate descriptor set: {}.", string_VkResult(allocate_result)));
    }
    return result_descriptor_set;
}

RODescriptorSetVK::RODescriptorSetVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder)
    : RenderObjectVK(p_render_data), variable_descriptor_count(p_builder.variable_descriptor_count),
      descriptor_set_layouts(p_render_data->allocator), bound_objects(p_render_data->allocator) {
    descriptor_set = p_builder.create_vk_object(p_render_data);
    descriptor_set_layouts.reserve(p_builder.descriptor_set_layouts.size());
    for (const auto& layout : p_builder.descriptor_set_layouts) {
        descriptor_set_layouts.push_back(layout);
    }
}

RODescriptorSetVK::~RODescriptorSetVK() {
    retire_descriptor_set(descriptor_set);
}

VkDescriptorSet RODescriptorSetVK::create_descriptor_set() const {
    Builder builder;
    for (const auto& descriptor_set_layout : descriptor_set_layouts) {
        builder.add_descriptor_set_layout(descriptor_set_layout);
    }
    if (variable_descriptor_count != 0) {
        builder.set_variable_descriptor_count(variable_descriptor_count);
    }
    return builder.create_vk_object(render_data);
}

void RODescriptorSetVK::retire_descriptor_set(VkDescriptorSet p_descriptor_set) const {
    if (p_descriptor_set == VK_NULL_HANDLE) {
        return;
    }
    render_data->register_delayed_call(
        [render_data = render_data, descriptor_set = p_descriptor_set]() {
            vkFreeDescriptorSets(render_data->logic_device, render_data->descriptor_pool, 1, &descriptor_set);
        },
        render_data->frames_in_flight);
}

void RODescriptorSetVK::validate_writer(const Writer& p_writer) const {
    size_t resource_type_count = 0;
    if (!p_writer.storage_buffer_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.uniform_buffer_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.sampled_image_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.sampler_infos.empty()) {
        resource_type_count++;
    }
    if (!p_writer.combined_image_sampler_infos.empty()) {
        resource_type_count++;
    }
    if (resource_type_count != 1) {
        throw std::runtime_error("Failed to write descriptor set: only one "
                                 "type of resource can be written at a time.");
    }
}

VkDescriptorSet RODescriptorSetVK::create_descriptor_set_for_write() const {
    if (!has_been_acquired) {
        return descriptor_set;
    }
    VkDescriptorSet result = create_descriptor_set();
    std::vector<VkCopyDescriptorSet> copy_infos;
    copy_infos.reserve(bound_objects.size());
    for (const auto& [key, objects] : bound_objects) {
        static_cast<void>(objects);
        uint32_t binding = static_cast<uint32_t>(key >> 32U);
        uint32_t array_element = static_cast<uint32_t>(key);
        copy_infos.push_back(VkCopyDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
            .srcSet = descriptor_set,
            .srcBinding = binding,
            .srcArrayElement = array_element,
            .dstSet = result,
            .dstBinding = binding,
            .dstArrayElement = array_element,
            .descriptorCount = 1,
        });
    }
    if (!copy_infos.empty()) {
        vkUpdateDescriptorSets(render_data->logic_device, 0, nullptr, static_cast<uint32_t>(copy_infos.size()), copy_infos.data());
    }
    return result;
}

void RODescriptorSetVK::publish_descriptor_set(VkDescriptorSet p_descriptor_set) {
    if (p_descriptor_set == descriptor_set) {
        return;
    }
    retire_descriptor_set(descriptor_set);
    descriptor_set = p_descriptor_set;
    has_been_acquired = false;
}

void RODescriptorSetVK::write_descriptor_set(const Writer& p_writer) {
    validate_writer(p_writer);
    VkDescriptorSet updated_descriptor_set = create_descriptor_set_for_write();

    VkWriteDescriptorSet write_info{};
    write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_info.dstSet = updated_descriptor_set;
    write_info.dstBinding = WBE_REQUIRED(p_writer.binding);
    write_info.dstArrayElement = p_writer.dst_array_element;
    PendingBoundObjectsVK pending_bound_objects(render_data->allocator);
    std::vector<VkDescriptorBufferInfo> buffer_infos;

    if (!p_writer.storage_buffer_infos.empty()) {
        write_buffer_info(
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, write_info, p_writer.storage_buffer_infos, render_data->allocator, pending_bound_objects, buffer_infos);
    }
    if (!p_writer.uniform_buffer_infos.empty()) {
        write_buffer_info(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, write_info, p_writer.uniform_buffer_infos, render_data->allocator, pending_bound_objects, buffer_infos);
    }
    std::vector<VkDescriptorImageInfo> image_infos;
    if (!p_writer.sampled_image_infos.empty()) {
        write_image_info(
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, write_info, p_writer.sampled_image_infos, render_data->allocator, pending_bound_objects, image_infos);
    }
    if (!p_writer.sampler_infos.empty()) {
        write_image_info(
            VK_DESCRIPTOR_TYPE_SAMPLER, write_info, p_writer.sampler_infos, render_data->allocator, pending_bound_objects, image_infos);
    }
    if (!p_writer.combined_image_sampler_infos.empty()) {
        write_image_info(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            write_info,
            p_writer.combined_image_sampler_infos,
            render_data->allocator,
            pending_bound_objects,
            image_infos);
    }
    // TODO: Texel buffer view
    vkUpdateDescriptorSets(render_data->logic_device, 1, &write_info, 0, nullptr);

    publish_descriptor_set(updated_descriptor_set);
    for (uint32_t i = 0; i < pending_bound_objects.size(); ++i) {
        uint64_t key = descriptor_array_element_key(write_info.dstBinding, write_info.dstArrayElement + i);
        auto emplace_result = bound_objects.emplace(key, Vector<Ref<RenderObjectVK>, RenderAllocator>(render_data->allocator));
        auto& retained_objects = emplace_result.first->second;
        retained_objects.clear();
        retained_objects.reserve(pending_bound_objects[i].size());
        for (const auto& bound_object : pending_bound_objects[i]) {
            retained_objects.emplace_back(bound_object);
        }
    }
}

} // namespace WhiteBirdEngine
