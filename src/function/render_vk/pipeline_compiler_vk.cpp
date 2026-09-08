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
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_pipeline_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "generated/type_uuid.gen.hh"
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

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
    p_pipeline->set_descriptor_layout(p_layout);
    for (const ShaderDescriptorHeapFieldVK& field : p_layout.fields) {
        Ref<IRenderObject> resource = MEM_NULL;
        uint32_t descriptor_index = 0;
        bool binding_found = false;
        if (field.type == ShaderDescriptorHeapTypeVK::UniformBuffer || field.type == ShaderDescriptorHeapTypeVK::StorageBuffer) {
            for (const RenderBindpointBufferDeclaration& declaration : p_bindpoint_declarations.buffer_declerations) {
                if (declaration.field_path == field.field_path) {
                    binding_found = true;
                    resource = declaration.buffer_render_object;
                    if (declaration.offset != 0 || declaration.size != std::numeric_limits<size_t>::max()) {
                        throw std::runtime_error(std::format(
                            "Failed to bind descriptor heap buffer '{}': descriptor heap buffers must bind the complete resource.",
                            field.field_path));
                    }
                    break;
                }
            }
            auto buffer = resource.dynamic_cast_ref<ROBufferVK>();
            if (buffer == MEM_NULL) {
                if (!binding_found) {
                    continue;
                }
                throw std::runtime_error(
                    std::format("Failed to bind descriptor heap buffer '{}': resource type is invalid.", field.field_path));
            }
            descriptor_index = field.type == ShaderDescriptorHeapTypeVK::UniformBuffer ? buffer->get_uniform_view_index()
                                                                                       : buffer->get_storage_view_index();
        } else if (field.type == ShaderDescriptorHeapTypeVK::SampledImage || field.type == ShaderDescriptorHeapTypeVK::StorageImage) {
            for (const RenderBindpointImageDeclaration& declaration : p_bindpoint_declarations.image_declerations) {
                if (declaration.field_path == field.field_path) {
                    binding_found = true;
                    resource = declaration.image_render_object;
                    break;
                }
            }
            auto image_view = resource.dynamic_cast_ref<ROImageViewVK>();
            if (image_view == MEM_NULL) {
                if (!binding_found) {
                    continue;
                }
                throw std::runtime_error(
                    std::format("Failed to bind descriptor heap image '{}': resource type is invalid.", field.field_path));
            }
            descriptor_index = field.type == ShaderDescriptorHeapTypeVK::SampledImage ? image_view->get_sampled_view_index()
                                                                                       : image_view->get_storage_view_index();
        } else {
            for (const RenderBindpointSamplerDeclaration& declaration : p_bindpoint_declarations.sampler_declerations) {
                if (declaration.field_path == field.field_path) {
                    binding_found = true;
                    resource = declaration.sampler_render_object;
                    break;
                }
            }
            auto sampler = resource.dynamic_cast_ref<ROSamplerVK>();
            if (sampler == MEM_NULL) {
                if (!binding_found) {
                    continue;
                }
                throw std::runtime_error(
                    std::format("Failed to bind descriptor heap sampler '{}': resource type is invalid.", field.field_path));
            }
            descriptor_index = sampler->get_sampler_index();
        }
        p_pipeline->set_descriptor_index(field.offset, descriptor_index, resource);
    }
}

} // namespace WhiteBirdEngine
