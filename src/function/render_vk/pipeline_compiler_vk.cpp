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
#include "function/render_vk/pipeline_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/ro_acceleration_structure_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_pipeline_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

const RenderBindpointBufferDeclaration* find_buffer_declaration(
    std::string_view p_field_path, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const RenderBindpointBufferDeclaration& declaration : p_bindpoint_declarations.buffer_declerations) {
        if (declaration.field_path == p_field_path) {
            return &declaration;
        }
    }
    return nullptr;
}

const RenderBindpointImageDeclaration* find_image_declaration(
    std::string_view p_field_path, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const RenderBindpointImageDeclaration& declaration : p_bindpoint_declarations.image_declerations) {
        if (declaration.field_path == p_field_path) {
            return &declaration;
        }
    }
    return nullptr;
}

const RenderBindpointAccelerationStructureDeclaration* find_acceleration_structure_declaration(
    std::string_view p_field_path, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const RenderBindpointAccelerationStructureDeclaration& declaration :
        p_bindpoint_declarations.acceleration_structure_declerations) {
        if (declaration.field_path == p_field_path) {
            return &declaration;
        }
    }
    return nullptr;
}

const RenderBindpointSamplerDeclaration* find_sampler_declaration(
    std::string_view p_field_path, const RenderBindpointDeclarations& p_bindpoint_declarations) {
    for (const RenderBindpointSamplerDeclaration& declaration : p_bindpoint_declarations.sampler_declerations) {
        if (declaration.field_path == p_field_path) {
            return &declaration;
        }
    }
    return nullptr;
}

bool bind_buffer_descriptor_field(Ref<ROPipelineVK> p_pipeline,
    const ShaderDescriptorHeapFieldVK& p_field,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    const RenderBindpointBufferDeclaration* declaration = find_buffer_declaration(p_field.field_path, p_bindpoint_declarations);
    if (declaration == nullptr) {
        return false;
    }
    if (declaration->offset != 0 || declaration->size != std::numeric_limits<size_t>::max()) {
        throw std::runtime_error(std::format(
            "Failed to bind descriptor heap buffer '{}': descriptor heap buffers must bind the complete resource.", p_field.field_path));
    }
    if (declaration->buffer_render_object_container != MEM_NULL) {
        // Keep container-backed bindings on the per-frame resolution path.
        p_pipeline->set_descriptor_binding(p_field.field_path, declaration->buffer_render_object_container);
        return true;
    }
    Ref<ROBufferVK> buffer = declaration->buffer_render_object.dynamic_cast_ref<ROBufferVK>();
    if (buffer == MEM_NULL) {
        throw std::runtime_error(std::format(
            "Failed to bind descriptor heap buffer '{}': resource type is invalid.", p_field.field_path));
    }
    // Bind direct resources once so push-time rebinding can skip them.
    p_pipeline->set_descriptor_binding(p_field.field_path, declaration->buffer_render_object);
    return true;
}

bool bind_image_descriptor_field(Ref<ROPipelineVK> p_pipeline,
    const ShaderDescriptorHeapFieldVK& p_field,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    const RenderBindpointImageDeclaration* declaration = find_image_declaration(p_field.field_path, p_bindpoint_declarations);
    if (declaration == nullptr) {
        return false;
    }
    if (declaration->image_render_object_container != MEM_NULL) {
        // Keep container-backed bindings on the per-frame resolution path.
        p_pipeline->set_descriptor_binding(p_field.field_path, declaration->image_render_object_container);
        return true;
    }
    Ref<ROImageViewVK> image_view = declaration->image_render_object.dynamic_cast_ref<ROImageViewVK>();
    if (image_view == MEM_NULL) {
        throw std::runtime_error(
            std::format("Failed to bind descriptor heap image '{}': resource type is invalid.", p_field.field_path));
    }
    // Bind direct resources once so push-time rebinding can skip them.
    p_pipeline->set_descriptor_binding(p_field.field_path, declaration->image_render_object);
    return true;
}

bool bind_acceleration_structure_descriptor_field(Ref<ROPipelineVK> p_pipeline,
    const ShaderDescriptorHeapFieldVK& p_field,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    const RenderBindpointAccelerationStructureDeclaration* declaration =
        find_acceleration_structure_declaration(p_field.field_path, p_bindpoint_declarations);
    if (declaration == nullptr) {
        return false;
    }
    if (declaration->acceleration_structure_render_object_container != MEM_NULL) {
        // Keep container-backed bindings on the per-frame resolution path.
        p_pipeline->set_descriptor_binding(p_field.field_path, declaration->acceleration_structure_render_object_container);
        return true;
    }
    Ref<ROAccelerationStructureVK> acceleration_structure =
        declaration->acceleration_structure_render_object.dynamic_cast_ref<ROAccelerationStructureVK>();
    if (acceleration_structure == MEM_NULL) {
        throw std::runtime_error(std::format(
            "Failed to bind descriptor heap acceleration structure '{}': resource type is invalid.", p_field.field_path));
    }
    // Bind direct resources once so push-time rebinding can skip them.
    p_pipeline->set_descriptor_binding(p_field.field_path, declaration->acceleration_structure_render_object);
    return true;
}

bool bind_sampler_descriptor_field(Ref<ROPipelineVK> p_pipeline,
    const ShaderDescriptorHeapFieldVK& p_field,
    const RenderBindpointDeclarations& p_bindpoint_declarations) {
    const RenderBindpointSamplerDeclaration* declaration = find_sampler_declaration(p_field.field_path, p_bindpoint_declarations);
    if (declaration == nullptr) {
        return false;
    }
    if (declaration->sampler_render_object_container != MEM_NULL) {
        // Keep container-backed bindings on the per-frame resolution path.
        p_pipeline->set_descriptor_binding(p_field.field_path, declaration->sampler_render_object_container);
        return true;
    }
    Ref<ROSamplerVK> sampler = declaration->sampler_render_object.dynamic_cast_ref<ROSamplerVK>();
    if (sampler == MEM_NULL) {
        throw std::runtime_error(
            std::format("Failed to bind descriptor heap sampler '{}': resource type is invalid.", p_field.field_path));
    }
    // Bind direct resources once so push-time rebinding can skip them.
    p_pipeline->set_descriptor_binding(p_field.field_path, declaration->sampler_render_object);
    return true;
}

} // namespace

PipelineCompilerVK::PipelineCompilerVK(const PipelineCompilerVKContext& p_context, std::string_view p_compiler_name)
    : base_context(p_context) {
    if (base_context.allocator == nullptr || base_context.render_data == nullptr || base_context.render_database == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to create {} compiler: invalid context.", p_compiler_name));
    }
}

Ref<ROShaderVK> PipelineCompilerVK::compile_shader(RefRes<RUShader> p_shader) {
    if (p_shader == nullptr || p_shader->resource_key == nullptr) {
        throw std::runtime_error("Failed to compile shader: shader resource key was not propagated.");
    }
    std::string shader_key = p_shader->resource_key;
    auto registered_shader =
        get_registered_render_object<ROShaderVK>(base_context.render_database, shader_key, TypeUUID<ROShaderVK>::UUID);
    if (registered_shader != MEM_NULL) {
        return registered_shader;
    }
    auto shader = ROShaderVK::Builder().set_code(p_shader).create_ref(base_context.render_data);
    base_context.render_database->register_compiled_render_resource(shader_key, shader);
    return shader;
}

void PipelineCompilerVK::compile_descriptor_indices(Ref<ROPipelineVK> p_pipeline,
    const ShaderDescriptorHeapLayoutVK& p_layout,
    const RenderBindpointDeclarations& p_bindpoint_declarations) const {
    if (p_pipeline == MEM_NULL) {
        throw std::runtime_error("Failed to compile pipeline descriptor indices: pipeline is missing.");
    }

    // Initialize the reflected descriptor slots before applying any bindings.
    p_pipeline->set_descriptor_layout(p_layout);

    for (const ShaderDescriptorHeapFieldVK& field : p_layout.fields) {
        if (field.type == ShaderDescriptorHeapTypeVK::UNIFORM_BUFFER || field.type == ShaderDescriptorHeapTypeVK::STORAGE_BUFFER) {
            // Buffer descriptors must resolve to either a per-frame container or one concrete buffer.
            bind_buffer_descriptor_field(p_pipeline, field, p_bindpoint_declarations);
        } else if (field.type == ShaderDescriptorHeapTypeVK::SAMPLED_IMAGE || field.type == ShaderDescriptorHeapTypeVK::STORAGE_IMAGE) {
            // Image descriptors follow the same direct-vs-container split as buffers.
            bind_image_descriptor_field(p_pipeline, field, p_bindpoint_declarations);
        } else if (field.type == ShaderDescriptorHeapTypeVK::ACCELERATION_STRUCTURE) {
            // Acceleration structures can be static or frame-varying, so keep both paths explicit.
            bind_acceleration_structure_descriptor_field(p_pipeline, field, p_bindpoint_declarations);
        } else {
            // Samplers use the same split, but resolve against sampler declarations.
            bind_sampler_descriptor_field(p_pipeline, field, p_bindpoint_declarations);
        }
    }
}

} // namespace WhiteBirdEngine
