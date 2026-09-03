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
#ifndef WBE_FILE_COMPUTE_PIPELINE_COMPILER_VK_HH
#define WBE_FILE_COMPUTE_PIPELINE_COMPILER_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/pipeline_compiler_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_compute_pipeline_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdint>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class ComputePipelineCompilerVKContext
 * @brief Runtime state required to compile a reflected compute shader.
 */
struct ComputePipelineCompilerVKContext {
    RenderAllocator* allocator = nullptr;
    const BaseRenderDataVK* render_data = nullptr;
    Ref<RenderDatabase> render_database = MEM_NULL;
};

/**
 * @class ComputePipelineCompilerVK
 * @brief Compiles a compute shader resource into Vulkan pipeline and descriptor objects.
 */
class ComputePipelineCompilerVK : public PipelineCompilerVK {
public:
    using DescriptorSetBinding = PipelineCompilerVK::DescriptorSetBinding;
    using DescriptorSetBindings = PipelineCompilerVK::DescriptorSetBindings;

    struct CompileResult {
        Ref<ROComputePipelineVK> compute_pipeline = MEM_NULL;
        DescriptorSetBindings descriptor_set_bindings;
    };

    explicit ComputePipelineCompilerVK(const ComputePipelineCompilerVKContext& p_context);

    CompileResult compile(std::string_view p_resource_key,
        RefRes<RUShader> p_shader_resource,
        const RenderBindpointDeclarations& p_bindpoint_declarations = {});

private:
    ComputePipelineCompilerVKContext context;

    Ref<ROComputePipelineVK> compile_compute_pipeline(std::string_view p_resource_key,
        Ref<ROShaderVK> p_compute_shader,
        std::string_view p_compute_entry,
        Ref<ROPipelineLayoutVK> p_pipeline_layout);
    std::vector<Ref<RODescriptorSetVK>> compile_descriptor_sets(const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        const RenderBindpointDeclarations& p_bindpoint_declarations) const;
};

} // namespace WhiteBirdEngine

#endif
