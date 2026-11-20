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
#ifndef WBE_FILE_PIPELINE_COMPILER_VK_HH
#define WBE_FILE_PIPELINE_COMPILER_VK_HH

#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/global_rend_cache.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_shader_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <string_view>

namespace WhiteBirdEngine {

class ROPipelineVK;

struct PipelineCompilerVKContext {
    RendAlloc* alloc = nullptr;
    const RendContextVK* rend_data = nullptr;
    Ref<GlobalRendCache> rend_cache = MEM_NULL;
};

class PipelineCompilerVK {
protected:
    explicit PipelineCompilerVK(const PipelineCompilerVKContext& p_context, std::string_view p_compiler_name);

    Ref<ROShaderVK> compile_shader(RefRes<RUShader> p_shader);

    PipelineCompilerVKContext base_context;
};

} // namespace WhiteBirdEngine

#endif
