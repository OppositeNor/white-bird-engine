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
#include "function/render_vk/ro_pipeline_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROPipelineVK::ROPipelineVK(const BaseRenderDataVK* p_render_data, Ref<ROPipelineLayoutVK> p_pipeline_layout)
    : RenderObjectVK(p_render_data), pipeline_layout(std::move(p_pipeline_layout)),
      descriptor_sets(p_render_data->allocator), descriptor_writes(p_render_data->allocator),
      descriptor_set_instances(p_render_data->allocator) {
    if (pipeline_layout == MEM_NULL) {
        throw std::runtime_error("Failed to create Vulkan pipeline: pipeline layout is missing.");
    }
}

ROPipelineVK::~ROPipelineVK() {
    if (pipeline == VK_NULL_HANDLE) {
        return;
    }
    render_data->register_delayed_call(
        [render_data = render_data, pipeline = pipeline]() {
            vkDestroyPipeline(render_data->logic_device, pipeline, render_data->allocation_callbacks);
        },
        render_data->frames_in_flight);
}

VkPipelineLayout ROPipelineVK::get_pipeline_layout() const {
    return pipeline_layout->get_pipeline_layout();
}

void ROPipelineVK::write_descriptor_set(uint32_t p_set_index, const RODescriptorSetVK::Writer& p_writer) {
    if (p_set_index >= descriptor_set_count) {
        throw std::runtime_error("Failed to write pipeline descriptor set: set index is out of bounds.");
    }
    const uint32_t binding = WBE_REQUIRED(p_writer.binding);
    for (DescriptorWrite& descriptor_write : descriptor_writes) {
        if (descriptor_write.set_index != p_set_index || descriptor_write.binding != binding ||
            descriptor_write.dst_array_element != p_writer.dst_array_element) {
            continue;
        }
        descriptor_write.writer = p_writer;
        descriptor_write.generation = ++descriptor_generation;
        return;
    }
    DescriptorWrite descriptor_write(render_data->allocator);
    descriptor_write.set_index = p_set_index;
    descriptor_write.binding = binding;
    descriptor_write.dst_array_element = p_writer.dst_array_element;
    descriptor_write.generation = ++descriptor_generation;
    descriptor_write.writer = p_writer;
    descriptor_write.applied_generations.resize(render_data->frames_in_flight, 0);
    descriptor_writes.push_back(std::move(descriptor_write));
}

void ROPipelineVK::bind_descriptor_sets(VkCommandBuffer p_command_buffer, VkPipelineBindPoint p_bind_point) {
    const uint32_t frame_slot = render_data->current_frame_slot;
    synchronize_descriptor_sets(frame_slot);
    descriptor_set_instances.clear();
    for (uint32_t set_index = 0; set_index < descriptor_set_count; ++set_index) {
        Ref<RODescriptorSetVK> descriptor_set = get_descriptor_set(frame_slot, set_index);
        if (descriptor_set == MEM_NULL) {
            throw std::runtime_error("Failed to bind pipeline descriptor sets: descriptor set is missing.");
        }
        descriptor_set_instances.push_back(descriptor_set->get_descriptor_set());
    }
    if (descriptor_set_instances.empty()) {
        return;
    }
    vkCmdBindDescriptorSets(p_command_buffer,
        p_bind_point,
        get_pipeline_layout(),
        0,
        static_cast<uint32_t>(descriptor_set_instances.size()),
        descriptor_set_instances.data(),
        0,
        nullptr);
}

void ROPipelineVK::set_descriptor_sets(std::vector<Ref<RODescriptorSetVK>> p_descriptor_sets) {
    descriptor_set_count = static_cast<uint32_t>(p_descriptor_sets.size());
    descriptor_sets.clear();
    descriptor_writes.clear();
    descriptor_generation = 0;
    descriptor_sets.reserve(static_cast<size_t>(descriptor_set_count) * render_data->frames_in_flight);
    descriptor_sets.insert(
        descriptor_sets.end(), std::make_move_iterator(p_descriptor_sets.begin()), std::make_move_iterator(p_descriptor_sets.end()));
    for (uint32_t frame_slot = 1; frame_slot < render_data->frames_in_flight; ++frame_slot) {
        for (uint32_t set_index = 0; set_index < descriptor_set_count; ++set_index) {
            descriptor_sets.push_back(get_descriptor_set(0, set_index)->create_copy());
        }
    }
    descriptor_set_instances.clear();
    descriptor_set_instances.reserve(descriptor_set_count);
}

Ref<RODescriptorSetVK> ROPipelineVK::get_descriptor_set(uint32_t p_frame_slot, uint32_t p_set_index) const {
    if (p_frame_slot >= render_data->frames_in_flight || p_set_index >= descriptor_set_count) {
        throw std::runtime_error("Failed to access pipeline descriptor set: frame slot or set index is out of bounds.");
    }
    return descriptor_sets[static_cast<size_t>(p_frame_slot) * descriptor_set_count + p_set_index];
}

void ROPipelineVK::synchronize_descriptor_sets(uint32_t p_frame_slot) {
    std::vector<DescriptorWrite*> pending_writes;
    for (DescriptorWrite& descriptor_write : descriptor_writes) {
        if (descriptor_write.applied_generations[p_frame_slot] == descriptor_write.generation) {
            // Contextual resources have their own content generations. Resolve them even when the descriptor binding itself is
            // current so ROCPerFrameVK can apply newly published bytes to the selected frame-slot buffer.
            if (descriptor_write.writer.has_contextual_resources()) {
                static_cast<void>(descriptor_write.writer.resolve(p_frame_slot));
            }
            continue;
        }
        pending_writes.push_back(&descriptor_write);
    }
    std::sort(pending_writes.begin(), pending_writes.end(), [](const DescriptorWrite* p_lhs, const DescriptorWrite* p_rhs) {
        return p_lhs->generation < p_rhs->generation;
    });
    for (DescriptorWrite* descriptor_write : pending_writes) {
        RODescriptorSetVK::Writer resolved_writer = descriptor_write->writer.resolve(p_frame_slot);
        Ref<RODescriptorSetVK> descriptor_set = get_descriptor_set(p_frame_slot, descriptor_write->set_index);
        descriptor_set->write_descriptor_set(resolved_writer);
        descriptor_write->applied_generations[p_frame_slot] = descriptor_write->generation;
    }
}

} // namespace WhiteBirdEngine
