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
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render/shader_metadata_query.hh"
#include "function/render_vk/pipeline_compiler_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

struct IndexedFieldPath {
    std::string field_path;
    std::optional<uint32_t> array_index;
};

IndexedFieldPath parse_indexed_field_path(std::string_view p_field_path) {
    size_t open_index = p_field_path.find('[');
    size_t stray_close_index = p_field_path.find(']');
    if (open_index == std::string_view::npos) {
        if (stray_close_index != std::string_view::npos) {
            throw std::runtime_error(std::format("Failed to parse bindpoint '{}': closing bracket without opening bracket.", p_field_path));
        }
        return IndexedFieldPath{.field_path = std::string(p_field_path)};
    }
    size_t close_index = p_field_path.find(']', open_index + 1);
    if (close_index == std::string_view::npos) {
        throw std::runtime_error(std::format("Failed to parse bindpoint '{}': missing closing bracket.", p_field_path));
    }
    if (close_index + 1 < p_field_path.size() && p_field_path[close_index + 1] != '.') {
        throw std::runtime_error(
            std::format("Failed to parse bindpoint '{}': array index must be followed by a field separator.", p_field_path));
    }
    uint32_t array_index = 0;
    for (char character : p_field_path.substr(open_index + 1, close_index - open_index - 1)) {
        if (character < '0' || character > '9') {
            throw std::runtime_error(std::format("Failed to parse bindpoint '{}': array index is invalid.", p_field_path));
        }
        array_index = array_index * 10 + static_cast<uint32_t>(character - '0');
    }
    std::string result;
    result.reserve(p_field_path.size());
    result.append(p_field_path.substr(0, open_index));
    result.append(p_field_path.substr(close_index + 1));
    return IndexedFieldPath{.field_path = std::move(result), .array_index = array_index};
}

void validate_bindpoint_declarations(const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile compute pipeline: uniform binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
        if (bindpoint_declaration.offset + bindpoint_declaration.size > p_bindpoint_declarations.buffer_data.size()) {
            throw std::runtime_error(std::format("Failed to compile compute pipeline: uniform binding '{}' exceeds the packed buffer range.",
                bindpoint_declaration.field_path));
        }
    }
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile compute pipeline: image binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
    }
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
        if (bindpoint_declaration.field_path.empty()) {
            throw std::runtime_error("Failed to compile compute pipeline: sampler binding field path is empty.");
        }
        parse_indexed_field_path(bindpoint_declaration.field_path);
    }
}

std::string get_stage_entry(RefRes<char> p_stage_entry) {
    if (p_stage_entry == nullptr || p_stage_entry.get()[0] == '\0') {
        return {};
    }
    return std::string(p_stage_entry.get());
}

std::string resolve_compute_stage_entry(RefRes<RUShader> p_shader_resource, const ShaderMetadata& p_metadata) {
    std::string compute_entry = get_stage_entry(p_shader_resource->compute_entry);
    return std::string(ShaderMetadataQuery().resolve_entry_point(p_metadata, "compute", compute_entry).name);
}

const RenderBindpointBufferDeclaration* find_uniform_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        if (bindpoint_declaration.field_path == p_field_path) {
            return &bindpoint_declaration;
        }
    }
    return nullptr;
}

const RenderBindpointImageDeclaration* find_image_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index, bool p_has_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path != p_field_path) {
            continue;
        }
        if (indexed_path.array_index.has_value() != p_has_array_index) {
            continue;
        }
        if (indexed_path.array_index.has_value() && *indexed_path.array_index != p_array_index) {
            continue;
        }
        return &bindpoint_declaration;
    }
    if (p_has_array_index && p_array_index > 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
            IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    if (!p_has_array_index && p_array_index == 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.image_declerations) {
            IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    return nullptr;
}

const RenderBindpointSamplerDeclaration* find_sampler_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index, bool p_has_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path != p_field_path) {
            continue;
        }
        if (indexed_path.array_index.has_value() != p_has_array_index) {
            continue;
        }
        if (indexed_path.array_index.has_value() && *indexed_path.array_index != p_array_index) {
            continue;
        }
        return &bindpoint_declaration;
    }
    if (p_has_array_index && p_array_index > 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
            IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    if (!p_has_array_index && p_array_index == 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.sampler_declerations) {
            IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    return nullptr;
}

const RenderBindpointBufferDeclaration* find_uniform_binding(
    const RenderBindpointDeclarations& p_bindpoint_declarations, std::string_view p_field_path, uint32_t p_array_index, bool p_has_array_index) {
    for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
        IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
        if (indexed_path.field_path != p_field_path) {
            continue;
        }
        if (indexed_path.array_index.has_value() != p_has_array_index) {
            continue;
        }
        if (indexed_path.array_index.has_value() && *indexed_path.array_index != p_array_index) {
            continue;
        }
        return &bindpoint_declaration;
    }
    if (p_has_array_index && p_array_index > 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
            IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    if (!p_has_array_index && p_array_index == 0) {
        for (const auto& bindpoint_declaration : p_bindpoint_declarations.buffer_declerations) {
            IndexedFieldPath indexed_path = parse_indexed_field_path(bindpoint_declaration.field_path);
            if (indexed_path.field_path == p_field_path && indexed_path.array_index.has_value() && *indexed_path.array_index == 0) {
                return &bindpoint_declaration;
            }
        }
    }
    return nullptr;
}

std::vector<uint8_t> build_uniform_data(const ShaderDescriptorBindingVK& p_descriptor_binding,
    const RenderBindpointDeclarations& p_bindpoint_declarations,
    uint32_t p_array_index,
    bool p_has_array_index) {
    if (p_descriptor_binding.buffer_size == 0) {
        throw std::runtime_error(std::format("Failed to compile compute uniform descriptor '{}': reflected buffer size is missing.",
            p_descriptor_binding.parameter_name));
    }
    std::vector<uint8_t> uniform_data(p_descriptor_binding.buffer_size, 0);
    if (const auto* direct_bindpoint_declaration =
            find_uniform_binding(p_bindpoint_declarations, p_descriptor_binding.parameter_name, p_array_index, p_has_array_index);
        direct_bindpoint_declaration != nullptr) {
        std::memcpy(uniform_data.data(),
            p_bindpoint_declarations.buffer_data.data() + direct_bindpoint_declaration->offset,
            std::min(direct_bindpoint_declaration->size, uniform_data.size()));
        return uniform_data;
    }
    for (const auto& field : p_descriptor_binding.fields) {
        const auto* bindpoint_declaration = find_uniform_binding(p_bindpoint_declarations, field.field_path);
        if (bindpoint_declaration == nullptr) {
            continue;
        }
        if (bindpoint_declaration->size != field.size || field.offset + field.size > uniform_data.size()) {
            throw std::runtime_error(
                std::format("Failed to bind compute uniform field '{}': incompatible reflected size.", field.field_path));
        }
        std::memcpy(uniform_data.data() + field.offset,
            p_bindpoint_declarations.buffer_data.data() + bindpoint_declaration->offset,
            field.size);
    }
    return uniform_data;
}

} // namespace

ComputePipelineCompilerVK::ComputePipelineCompilerVK(const ComputePipelineCompilerVKContext& p_context)
    : PipelineCompilerVK(
          PipelineCompilerVKContext{
              .allocator = p_context.allocator,
              .render_data = p_context.render_data,
              .render_database = p_context.render_database,
          },
          "compute pipeline"),
      context(p_context) {
}

ComputePipelineCompilerVK::CompileResult ComputePipelineCompilerVK::compile(
    std::string_view p_resource_key, RefRes<RUShader> p_shader_resource, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    validate_bindpoint_declarations(p_bindpoint_declarations);
    if (p_shader_resource == nullptr || p_shader_resource->resource_key == nullptr) {
        throw std::runtime_error("Failed to compile compute pipeline: shader resource key was not propagated.");
    }
    Ref<ROShaderVK> compute_shader = compile_shader(p_shader_resource);
    ShaderMetadata shader_metadata = ShaderMetadataLoader().load(p_shader_resource);
    std::string compute_entry = resolve_compute_stage_entry(p_shader_resource, shader_metadata);
    std::vector<ShaderDescriptorBindingVK> descriptor_bindings =
        collect_shader_descriptor_bindings(shader_metadata, "compute", compute_entry);
    std::vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts =
        compile_descriptor_set_layouts(p_resource_key, descriptor_bindings);
    Ref<ROPipelineLayoutVK> pipeline_layout = compile_pipeline_layout(p_resource_key, descriptor_set_layouts);
    Ref<ROComputePipelineVK> compute_pipeline = compile_compute_pipeline(p_resource_key, compute_shader, compute_entry, pipeline_layout);
    compute_pipeline->set_descriptor_sets(compile_descriptor_sets(descriptor_set_layouts, descriptor_bindings, p_bindpoint_declarations));
    return CompileResult{
        .compute_pipeline = compute_pipeline, .descriptor_set_bindings = build_descriptor_set_bindings(descriptor_bindings)};
}

Ref<ROComputePipelineVK> ComputePipelineCompilerVK::compile_compute_pipeline(
    std::string_view p_resource_key, Ref<ROShaderVK> p_compute_shader, std::string_view p_compute_entry, Ref<ROPipelineLayoutVK> p_pipeline_layout) {
    // Descriptor sets are pipeline-owned, so each compile result needs an independent runtime pipeline object. Shared
    // shaders and layouts are still cached by PipelineCompilerVK.
    ROComputePipelineVK::Builder pipeline_builder;
    pipeline_builder.set_shader_stage(p_compute_shader, p_compute_entry).set_pipeline_layout(p_pipeline_layout);
    Ref<ROComputePipelineVK> compute_pipeline = pipeline_builder.create_ref(context.render_data);
    context.render_database->register_compiled_render_resource(
        p_resource_key, TypeUUID<ROComputePipelineVK>::UUID, RefWeak<IRenderObject>(compute_pipeline));
    return compute_pipeline;
}

std::vector<Ref<RODescriptorSetVK>> ComputePipelineCompilerVK::compile_descriptor_sets(
    const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
    descriptor_sets.reserve(p_descriptor_set_layouts.size());
    for (uint32_t set_index = 0; set_index < p_descriptor_set_layouts.size(); ++set_index) {
        Ref<RODescriptorSetVK> descriptor_set =
            RODescriptorSetVK::Builder().add_descriptor_set_layout(p_descriptor_set_layouts[set_index]).create_ref(context.render_data);
        write_uniform_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_bindpoint_declarations);
        write_image_descriptors(descriptor_set, p_descriptor_bindings, set_index, p_bindpoint_declarations);
        descriptor_sets.push_back(descriptor_set);
    }
    return descriptor_sets;
}

void ComputePipelineCompilerVK::write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index ||
            descriptor_binding.layout_binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            continue;
        }
        uint32_t descriptor_count = std::max(descriptor_binding.layout_binding.descriptorCount, 1U);
        bool has_array_index = descriptor_binding.layout_binding.descriptorCount > 1;
        for (uint32_t array_index = 0; array_index < descriptor_count; ++array_index) {
            std::vector<uint8_t> uniform_data =
                build_uniform_data(descriptor_binding, p_bindpoint_declarations, array_index, has_array_index);
            Ref<ROBufferVK> uniform_buffer = ROBufferVK::Builder()
                                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                                 .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                 .set_size(uniform_data.size())
                                                 .create_ref(context.render_data);
            uniform_buffer->update_buffer(uniform_data.data(), uniform_data.size());
            p_descriptor_set->write_descriptor_set(RODescriptorSetVK::Writer()
                    .set_binding(descriptor_binding.layout_binding.binding)
                    .set_dst_array_element(array_index)
                    .add_uniform_buffer_info(uniform_buffer, 0, descriptor_binding.buffer_size));
        }
    }
}

void ComputePipelineCompilerVK::write_image_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
    uint32_t p_set_index,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        if (descriptor_binding.set_index != p_set_index) {
            continue;
        }
        VkDescriptorType descriptor_type = descriptor_binding.layout_binding.descriptorType;
        if (descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && descriptor_type != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
            descriptor_type != VK_DESCRIPTOR_TYPE_SAMPLER) {
            continue;
        }
        RODescriptorSetVK::Writer writer;
        writer.set_binding(descriptor_binding.layout_binding.binding);
        uint32_t descriptor_count = std::max(descriptor_binding.layout_binding.descriptorCount, 1U);
        for (uint32_t array_index = 0; array_index < descriptor_count; ++array_index) {
            bool has_array_index = descriptor_binding.layout_binding.descriptorCount > 1;
            if (descriptor_type == VK_DESCRIPTOR_TYPE_SAMPLER) {
                const auto* sampler_binding =
                    find_sampler_binding(p_bindpoint_declarations, descriptor_binding.parameter_name, array_index, has_array_index);
                if (sampler_binding == nullptr) {
                    throw std::runtime_error(std::format("Failed to bind compute sampler '{}'.", descriptor_binding.parameter_name));
                }
                auto sampler = sampler_binding->sampler_render_object.dynamic_cast_ref<ROSamplerVK>();
                if (sampler == MEM_NULL) {
                    throw std::runtime_error(std::format("Failed to bind compute sampler '{}': sampler object is missing.",
                        descriptor_binding.parameter_name));
                }
                writer.add_sampler_info({.sampler = sampler, .image_layout = VK_IMAGE_LAYOUT_GENERAL});
                continue;
            }
            const auto* image_binding =
                find_image_binding(p_bindpoint_declarations, descriptor_binding.parameter_name, array_index, has_array_index);
            if (image_binding == nullptr) {
                throw std::runtime_error(std::format("Failed to bind compute image '{}'.", descriptor_binding.parameter_name));
            }
            auto image_view = image_binding->image_render_object.dynamic_cast_ref<ROImageViewVK>();
            if (image_view == MEM_NULL) {
                throw std::runtime_error(std::format("Failed to bind compute image '{}': image view render object is missing.",
                    descriptor_binding.parameter_name));
            }
            if (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                writer.add_storage_image_info({.image_view = image_view, .image_layout = VK_IMAGE_LAYOUT_GENERAL});
            } else {
                writer.add_sampled_image_info({.image_view = image_view, .image_layout = VK_IMAGE_LAYOUT_GENERAL});
            }
        }
        p_descriptor_set->write_descriptor_set(writer);
    }
}

} // namespace WhiteBirdEngine
