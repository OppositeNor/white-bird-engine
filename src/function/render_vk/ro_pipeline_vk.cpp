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
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_render_object_container.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROPipelineVK::ROPipelineVK(const BaseRenderDataVK* p_render_data)
    : IRenderObjectVK(p_render_data), descriptor_index_data(p_render_data->allocator),
      descriptor_resources(p_render_data->allocator), descriptor_resource_containers(p_render_data->allocator),
      descriptor_fields(p_render_data->allocator) {
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

void ROPipelineVK::set_descriptor_layout(const ShaderDescriptorHeapLayoutVK& p_layout) {
    if (p_layout.size > render_data->descriptor_heap_properties.maxPushDataSize) {
        throw std::runtime_error("Failed to set pipeline descriptor layout: reflected push data exceeds the device limit.");
    }
    descriptor_fields.assign(p_layout.fields.begin(), p_layout.fields.end());
    descriptor_index_data.assign(p_layout.size, 0);
    descriptor_resources.assign(p_layout.fields.size(), MEM_NULL);
    descriptor_resource_containers.assign(p_layout.fields.size(), MEM_NULL);
}

void ROPipelineVK::retain_descriptor_resource(Ref<IRenderObject> p_resource) {
    if (p_resource == MEM_NULL) {
        throw std::runtime_error("Failed to retain pipeline descriptor resource: resource is missing.");
    }
    descriptor_resources.push_back(std::move(p_resource));
}

void ROPipelineVK::set_descriptor_index(uint32_t p_offset, uint32_t p_index, Ref<IRenderObject> p_resource) {
    constexpr size_t DESCRIPTOR_INDEX_SIZE = sizeof(uint32_t) * 2;
    if (p_offset > descriptor_index_data.size() || DESCRIPTOR_INDEX_SIZE > descriptor_index_data.size() - p_offset) {
        throw std::runtime_error("Failed to set pipeline descriptor index: reflected push-data offset is out of bounds.");
    }
    if (p_resource == MEM_NULL) {
        throw std::runtime_error("Failed to set pipeline descriptor index: resource is missing.");
    }
    const uint32_t descriptor_index[2] = {p_index, 0};
    std::memcpy(descriptor_index_data.data() + p_offset, descriptor_index, DESCRIPTOR_INDEX_SIZE);
    auto field = std::ranges::find_if(descriptor_fields, [p_offset](const ShaderDescriptorHeapFieldVK& p_field) {
        return p_field.offset == p_offset;
    });
    if (field == descriptor_fields.end()) {
        throw std::runtime_error("Failed to set pipeline descriptor index: reflected field is missing.");
    }
    descriptor_resources[static_cast<size_t>(field - descriptor_fields.begin())] = std::move(p_resource);
}

void ROPipelineVK::set_descriptor_binding(std::string_view p_field_path, Ref<IRenderObject> p_resource) {
    auto field = std::ranges::find_if(descriptor_fields, [p_field_path](const ShaderDescriptorHeapFieldVK& p_field) {
        return p_field.field_path == p_field_path;
    });
    if (field == descriptor_fields.end()) {
        throw std::runtime_error("Failed to set pipeline descriptor binding: reflected field path was not found.");
    }
    uint32_t index = 0;
    switch (field->type) {
    case ShaderDescriptorHeapTypeVK::UNIFORM_BUFFER: {
        Ref<ROBufferVK> buffer = p_resource.dynamic_cast_ref<ROBufferVK>();
        if (buffer == MEM_NULL) {
            throw std::runtime_error("Failed to set uniform-buffer descriptor binding: resource type is invalid.");
        }
        index = buffer->get_uniform_view_index();
        break;
    }
    case ShaderDescriptorHeapTypeVK::STORAGE_BUFFER: {
        Ref<ROBufferVK> buffer = p_resource.dynamic_cast_ref<ROBufferVK>();
        if (buffer == MEM_NULL) {
            throw std::runtime_error("Failed to set storage-buffer descriptor binding: resource type is invalid.");
        }
        index = buffer->get_storage_view_index();
        break;
    }
    case ShaderDescriptorHeapTypeVK::SAMPLED_IMAGE: {
        Ref<ROImageViewVK> image_view = p_resource.dynamic_cast_ref<ROImageViewVK>();
        if (image_view == MEM_NULL) {
            throw std::runtime_error("Failed to set sampled-image descriptor binding: resource type is invalid.");
        }
        index = image_view->get_sampled_view_index();
        break;
    }
    case ShaderDescriptorHeapTypeVK::STORAGE_IMAGE: {
        Ref<ROImageViewVK> image_view = p_resource.dynamic_cast_ref<ROImageViewVK>();
        if (image_view == MEM_NULL) {
            throw std::runtime_error("Failed to set storage-image descriptor binding: resource type is invalid.");
        }
        index = image_view->get_storage_view_index();
        break;
    }
    case ShaderDescriptorHeapTypeVK::SAMPLER: {
        Ref<ROSamplerVK> sampler = p_resource.dynamic_cast_ref<ROSamplerVK>();
        if (sampler == MEM_NULL) {
            throw std::runtime_error("Failed to set sampler descriptor binding: resource type is invalid.");
        }
        index = sampler->get_sampler_index();
        break;
    }
    case ShaderDescriptorHeapTypeVK::ACCELERATION_STRUCTURE: {
        Ref<ROAccelerationStructureVK> acceleration_structure = p_resource.dynamic_cast_ref<ROAccelerationStructureVK>();
        if (acceleration_structure == MEM_NULL) {
            throw std::runtime_error("Failed to set acceleration-structure descriptor binding: resource type is invalid.");
        }
        index = acceleration_structure->get_descriptor_index();
        break;
    }
    }
    set_descriptor_index(field->offset, index, std::move(p_resource));
    descriptor_resource_containers[static_cast<size_t>(field - descriptor_fields.begin())] = MEM_NULL;
}

void ROPipelineVK::set_descriptor_binding(std::string_view p_field_path, Ref<IRenderObjectContainer> p_resource_container) {
    if (p_resource_container == MEM_NULL) {
        throw std::runtime_error("Failed to set pipeline descriptor binding: resource container is missing.");
    }
    auto field = std::ranges::find_if(descriptor_fields, [p_field_path](const ShaderDescriptorHeapFieldVK& p_field) {
        return p_field.field_path == p_field_path;
    });
    if (field == descriptor_fields.end()) {
        throw std::runtime_error("Failed to set pipeline descriptor binding: reflected field path was not found.");
    }
    Ref<IRenderObjectVK> resource = p_resource_container->get(render_data->current_frame_slot).dynamic_cast_ref<IRenderObjectVK>();
    if (resource == MEM_NULL) {
        throw std::runtime_error("Failed to set pipeline descriptor binding: resource container did not resolve a Vulkan render object.");
    }
    set_descriptor_binding(p_field_path, resource);
    descriptor_resource_containers[static_cast<size_t>(field - descriptor_fields.begin())] = std::move(p_resource_container);
}

void ROPipelineVK::push_descriptor_indices(VkCommandBuffer p_command_buffer) {
    if (descriptor_index_data.empty()) {
        return;
    }
    for (size_t i = 0; i < descriptor_fields.size(); ++i) {
        if (descriptor_resource_containers[i] == MEM_NULL) {
            continue;
        }
        Ref<IRenderObjectVK> resource =
            descriptor_resource_containers[i]->get(render_data->current_frame_slot).dynamic_cast_ref<IRenderObjectVK>();
        if (resource == MEM_NULL) {
            throw std::runtime_error("Failed to push pipeline descriptor indices: resource container did not resolve a Vulkan render object.");
        }
        Ref<IRenderObjectContainer> container = descriptor_resource_containers[i];
        set_descriptor_binding(descriptor_fields[i].field_path, resource);
        descriptor_resource_containers[i] = std::move(container);
    }
    VkPushDataInfoEXT push_data_info{};
    push_data_info.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT;
    push_data_info.data.address = descriptor_index_data.data();
    push_data_info.data.size = descriptor_index_data.size();
    if (render_data->command_push_data == nullptr) {
        throw std::runtime_error("Failed to push pipeline descriptor indices: descriptor heap command is unavailable.");
    }
    render_data->command_push_data(p_command_buffer, &push_data_info);
}

} // namespace WhiteBirdEngine
