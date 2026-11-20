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
#include "function/rend_vk/pipeline_desc_bindings_vk.hh"
#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/i_rend_task.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_acceleration_structure_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/rend_vk/ro_image_view_vk.hh"
#include "function/rend_vk/ro_sampler_vk.hh"
#include "function/rend_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

RendResTID expected_resource_type(ShaderDescHeapTypeVK p_type) {
    switch (p_type) {
    case ShaderDescHeapTypeVK::UNIFORM_BUFFER:
    case ShaderDescHeapTypeVK::STORAGE_BUFFER:
        return TypeUUID<ROBufferVK>::UUID;
    case ShaderDescHeapTypeVK::SAMPLED_IMAGE:
    case ShaderDescHeapTypeVK::STORAGE_IMAGE:
        return TypeUUID<ROImageViewVK>::UUID;
    case ShaderDescHeapTypeVK::SAMPLER:
        return TypeUUID<ROSamplerVK>::UUID;
    case ShaderDescHeapTypeVK::ACCELERATION_STRUCTURE:
        return TypeUUID<ROAccelerationStructureVK>::UUID;
    }
    throw std::runtime_error("Failed to resolve pipeline descriptor binding: unknown descriptor type.");
}

std::optional<RendResUsageVK> access_usage(ShaderDescHeapTypeVK p_type) {
    switch (p_type) {
    case ShaderDescHeapTypeVK::UNIFORM_BUFFER:
        return RendResUsageVK::UNIFORM_BUFFER;
    case ShaderDescHeapTypeVK::STORAGE_BUFFER:
        return RendResUsageVK::STORAGE_BUFFER;
    case ShaderDescHeapTypeVK::SAMPLED_IMAGE:
        return RendResUsageVK::SAMPLED_IMAGE;
    case ShaderDescHeapTypeVK::STORAGE_IMAGE:
        return RendResUsageVK::STORAGE_IMAGE;
    case ShaderDescHeapTypeVK::ACCELERATION_STRUCTURE:
        return RendResUsageVK::ACCELERATION_STRUCTURE;
    case ShaderDescHeapTypeVK::SAMPLER:
        return std::nullopt;
    }
    throw std::runtime_error("Failed to resolve pipeline descriptor access: unknown descriptor type.");
}

uint32_t desc_index(ShaderDescHeapTypeVK p_type, Ref<IRendObject> p_resource, std::string_view p_resource_id) {
    switch (p_type) {
    case ShaderDescHeapTypeVK::UNIFORM_BUFFER: {
        Ref<ROBufferVK> buffer = p_resource.dynamic_cast_ref<ROBufferVK>();
        if (buffer != MEM_NULL) {
            return buffer->get_uniform_view_index();
        }
        break;
    }
    case ShaderDescHeapTypeVK::STORAGE_BUFFER: {
        Ref<ROBufferVK> buffer = p_resource.dynamic_cast_ref<ROBufferVK>();
        if (buffer != MEM_NULL) {
            return buffer->get_storage_view_index();
        }
        break;
    }
    case ShaderDescHeapTypeVK::SAMPLED_IMAGE: {
        Ref<ROImageViewVK> image_view = p_resource.dynamic_cast_ref<ROImageViewVK>();
        if (image_view != MEM_NULL) {
            return image_view->get_sampled_view_index();
        }
        break;
    }
    case ShaderDescHeapTypeVK::STORAGE_IMAGE: {
        Ref<ROImageViewVK> image_view = p_resource.dynamic_cast_ref<ROImageViewVK>();
        if (image_view != MEM_NULL) {
            return image_view->get_storage_view_index();
        }
        break;
    }
    case ShaderDescHeapTypeVK::SAMPLER: {
        Ref<ROSamplerVK> sampler = p_resource.dynamic_cast_ref<ROSamplerVK>();
        if (sampler != MEM_NULL) {
            return sampler->get_sampler_index();
        }
        break;
    }
    case ShaderDescHeapTypeVK::ACCELERATION_STRUCTURE: {
        Ref<ROAccelerationStructureVK> acceleration_structure = p_resource.dynamic_cast_ref<ROAccelerationStructureVK>();
        if (acceleration_structure != MEM_NULL) {
            return acceleration_structure->get_desc_index();
        }
        break;
    }
    }
    throw std::runtime_error(
        std::format("Failed to resolve pipeline descriptor binding '{}': concrete render-object type is invalid.", p_resource_id));
}

} // namespace

PipelineDescBindingsVK::PipelineDescBindingsVK(const RendContextVK* p_rend_data, const ShaderDescHeapLayoutVK& p_layout)
    : rend_data(p_rend_data), layout(p_layout), push_data(p_rend_data != nullptr ? p_rend_data->alloc : nullptr),
      containers(p_rend_data != nullptr ? p_rend_data->alloc : nullptr),
      custom_values_set(p_rend_data != nullptr ? p_rend_data->alloc : nullptr) {
    if (rend_data == nullptr || rend_data->alloc == nullptr) {
        throw std::runtime_error("Failed to construct pipeline descriptor bindings: render data is missing.");
    }
    push_data.resize(layout.size, uint8_t{0});
    containers.resize(layout.fields.size(), MEM_NULL);
    custom_values_set.resize(layout.default_fields.size(), false);
}

bool PipelineDescBindingsVK::try_set_push_constant(std::string_view p_name, PushConstantValueVK p_value) {
    bool declared = false;
    for (size_t i = 0; i < layout.default_fields.size(); ++i) {
        const ShaderDefaultPushFieldVK& field = layout.default_fields[i];
        if (field.value != ShaderDefaultPushValueVK::CUSTOM || field.name != p_name) {
            continue;
        }
        if (field.type != p_value.type) {
            throw std::runtime_error(std::format(
                "Failed to set push constant '{}': value type does not match reflected field '{}'.", p_name, field.field_path));
        }
        if (field.offset > push_data.size() || sizeof(p_value.bits) > push_data.size() - field.offset) {
            throw std::runtime_error("Failed to set push constant: reflected offset is out of bounds.");
        }
        std::memcpy(push_data.data() + field.offset, &p_value.bits, sizeof(p_value.bits));
        custom_values_set[i] = true;
        declared = true;
    }
    return declared;
}

void PipelineDescBindingsVK::set_push_constant(std::string_view p_name, PushConstantValueVK p_value) {
    if (!try_set_push_constant(p_name, p_value)) {
        throw std::runtime_error(std::format("Failed to set push constant '{}': pipeline does not declare it.", p_name));
    }
}

void PipelineDescBindingsVK::append_accesses(
    Vector<ResAccessVK, RendAlloc>& p_reads, Vector<ResAccessVK, RendAlloc>& p_writes) const {
    for (const ShaderDescHeapFieldVK& field : layout.fields) {
        std::optional<RendResUsageVK> usage = access_usage(field.type);
        if (!usage.has_value()) {
            continue;
        }
        if (field.access != ShaderDescAccessVK::WRITE) {
            p_reads.push_back({.res_id = field.resource_id, .usage = *usage});
        }
        if (field.access != ShaderDescAccessVK::READ) {
            p_writes.push_back({.res_id = field.resource_id, .usage = *usage});
        }
    }
}

void PipelineDescBindingsVK::bind_resources(const RendResViewVK& p_resources) {
    for (size_t i = 0; i < layout.fields.size(); ++i) {
        const ShaderDescHeapFieldVK& field = layout.fields[i];
        const RendResVK& resource = p_resources.get(field.resource_id);
        if (resource.res_type_id != expected_resource_type(field.type)) {
            throw std::runtime_error(std::format(
                "Failed to bind pipeline descriptor resource '{}': graph resource type does not match reflection.", field.resource_id));
        }
        if (resource.container == MEM_NULL) {
            throw std::runtime_error(
                std::format("Failed to bind pipeline descriptor resource '{}': container is null.", field.resource_id));
        }
        containers[i] = resource.container;
    }
}

void PipelineDescBindingsVK::push(VkCommandBuffer p_command_buffer) {
    if (layout.fields.empty() && layout.default_fields.empty()) {
        return;
    }
    constexpr size_t DESC_INDEX_SIZE = sizeof(uint32_t) * 2;
    for (size_t i = 0; i < layout.fields.size(); ++i) {
        const ShaderDescHeapFieldVK& field = layout.fields[i];
        if (containers[i] == MEM_NULL) {
            throw std::runtime_error(
                std::format("Failed to push pipeline descriptor resource '{}': binding was not resolved.", field.resource_id));
        }
        Ref<IRendObject> resource = containers[i]->get(rend_data->current_frame_slot);
        if (resource == MEM_NULL) {
            throw std::runtime_error(
                std::format("Failed to push pipeline descriptor resource '{}': current frame slot is null.", field.resource_id));
        }
        const uint32_t index_data[2] = {desc_index(field.type, resource, field.resource_id), 0};
        if (field.offset > push_data.size() || DESC_INDEX_SIZE > push_data.size() - field.offset) {
            throw std::runtime_error("Failed to push pipeline descriptor indices: reflected offset is out of bounds.");
        }
        std::memcpy(push_data.data() + field.offset, index_data, DESC_INDEX_SIZE);
    }
    constexpr size_t DEFAULT_PUSH_VALUE_SIZE = sizeof(uint32_t);
    for (size_t i = 0; i < layout.default_fields.size(); ++i) {
        const ShaderDefaultPushFieldVK& field = layout.default_fields[i];
        if (field.offset > push_data.size() || DEFAULT_PUSH_VALUE_SIZE > push_data.size() - field.offset) {
            throw std::runtime_error("Failed to push renderer default value: reflected offset is out of bounds.");
        }
        switch (field.value) {
        case ShaderDefaultPushValueVK::TIME: {
            const float time = static_cast<float>(rend_data->time);
            std::memcpy(push_data.data() + field.offset, &time, sizeof(time));
            break;
        }
        case ShaderDefaultPushValueVK::FRAME_SLOT:
            std::memcpy(push_data.data() + field.offset, &rend_data->current_frame_slot, sizeof(rend_data->current_frame_slot));
            break;
        case ShaderDefaultPushValueVK::CUSTOM:
            // Custom values are written into push data when set.
            if (!custom_values_set[i]) {
                throw std::runtime_error(
                    std::format("Failed to push pipeline data: custom push constant '{}' was not set.", field.name));
            }
            break;
        }
    }
    if (rend_data->command_push_data == nullptr) {
        throw std::runtime_error("Failed to push pipeline data: push-data command is unavailable.");
    }
    VkPushDataInfoEXT push_data_info{
        .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
        .data = {.address = push_data.data(), .size = push_data.size()},
    };
    rend_data->command_push_data(p_command_buffer, &push_data_info);
}

} // namespace WhiteBirdEngine
