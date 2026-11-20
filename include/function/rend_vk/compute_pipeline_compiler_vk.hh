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

#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_rend_cache.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/render/shader_macro_value.hh"
#include "function/rend_vk/pipeline_compiler_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_compute_pipeline_vk.hh"
#include "function/rend_vk/ro_shader_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class ComputePipelineCompilerVKContext
 * @brief Runtime state required to compile a reflected compute shader.
 */
struct ComputePipelineCompilerVKContext {
    RendAlloc* alloc = nullptr;
    const RendContextVK* rend_data = nullptr;
    Ref<GlobalRendCache> rend_cache = MEM_NULL;
};

/**
 * @class ComputePipelineCompilerVK
 * @brief Compiles a compute shader resource into Vulkan pipeline and descriptor objects.
 */
class ComputePipelineCompilerVK : public PipelineCompilerVK {
public:
    struct CompileResult {
        Ref<ROComputePipelineVK> compute_pipeline = MEM_NULL;
    };

    explicit ComputePipelineCompilerVK(const ComputePipelineCompilerVKContext& p_context);

    /**
     * @brief Compile a variant of a compute shader resource into a pipeline cached under p_resource_key.
     *
     * @param p_resource_key The pipeline's cache key.
     * @param p_shader_resource The compute shader resource.
     * @param p_variant Macro values selecting the shader's variant; empty selects the default variant.
     * @return The compiled pipeline.
     */
    CompileResult compile(std::string_view p_resource_key,
        RefRes<RUShader> p_shader_resource,
        const std::vector<ShaderMacroValue>& p_variant = {});

private:
    ComputePipelineCompilerVKContext context;

    Ref<ROComputePipelineVK> compile_compute_pipeline(std::string_view p_resource_key,
        Ref<ROShaderVK> p_compute_shader,
        std::string_view p_compute_entry,
        const std::vector<ShaderMacroValue>& p_variant);
};

} // namespace WhiteBirdEngine

#endif
