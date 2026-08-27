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
#include "core/memory/reference_weak.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <format>
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
    base_context.render_database->register_compiled_render_resource(
        shader_key, TypeUUID<ROShaderVK>::UUID, RefWeak<IRenderObject>(shader));
    return shader;
}

std::vector<Ref<RODescriptorSetLayoutVK>> PipelineCompilerVK::compile_descriptor_set_layouts(
    std::string_view p_resource_key, const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    std::vector<Ref<RODescriptorSetLayoutVK>> result;
    auto descriptor_set_layout_bindings = build_shader_descriptor_set_layout_bindings(p_descriptor_bindings);
    set_variable_descriptor_count_upper_bounds(
        descriptor_set_layout_bindings, base_context.render_data->max_variable_sampled_image_descriptor_count);
    result.reserve(descriptor_set_layout_bindings.size());
    for (const auto& descriptor_set_layout_binding : descriptor_set_layout_bindings) {
        std::string descriptor_set_layout_key =
            std::format("{}#descriptor_set_layout_{}", p_resource_key, descriptor_set_layout_binding.set_index);
        auto descriptor_set_layout = get_registered_render_object<RODescriptorSetLayoutVK>(
            base_context.render_database, descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID);
        if (descriptor_set_layout == MEM_NULL) {
            RODescriptorSetLayoutVK::Builder builder;
            for (size_t binding_index = 0; binding_index < descriptor_set_layout_binding.bindings.size(); ++binding_index) {
                builder.add_binding(descriptor_set_layout_binding.bindings[binding_index],
                    descriptor_set_layout_binding.binding_flags[binding_index]);
            }
            descriptor_set_layout = builder.create_ref(base_context.render_data);
            base_context.render_database->register_compiled_render_resource(
                descriptor_set_layout_key, TypeUUID<RODescriptorSetLayoutVK>::UUID, RefWeak<IRenderObject>(descriptor_set_layout));
        }
        result.push_back(descriptor_set_layout);
    }
    return result;
}

Ref<ROPipelineLayoutVK> PipelineCompilerVK::compile_pipeline_layout(
    std::string_view p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts) {
    std::string pipeline_layout_key = std::format("{}#pipeline_layout", p_resource_key);
    auto pipeline_layout = get_registered_render_object<ROPipelineLayoutVK>(
        base_context.render_database, pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID);
    if (pipeline_layout != MEM_NULL) {
        return pipeline_layout;
    }
    ROPipelineLayoutVK::Builder builder;
    for (const auto& descriptor_set_layout : p_descriptor_set_layouts) {
        builder.add_descriptor_set_layout(descriptor_set_layout);
    }
    pipeline_layout = builder.create_ref(base_context.render_data);
    base_context.render_database->register_compiled_render_resource(
        pipeline_layout_key, TypeUUID<ROPipelineLayoutVK>::UUID, RefWeak<IRenderObject>(pipeline_layout));
    return pipeline_layout;
}

PipelineCompilerVK::DescriptorSetBindings PipelineCompilerVK::build_descriptor_set_bindings(
    const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings) {
    DescriptorSetBindings result;
    for (const auto& descriptor_binding : p_descriptor_bindings) {
        result.emplace(descriptor_binding.parameter_name,
            DescriptorSetBinding{descriptor_binding.set_index, descriptor_binding.layout_binding.binding});
    }
    return result;
}

} // namespace WhiteBirdEngine
