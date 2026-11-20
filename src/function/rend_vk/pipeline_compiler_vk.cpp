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
#include "function/rend_vk/pipeline_compiler_vk.hh"
#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/rend_vk/ro_shader_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

PipelineCompilerVK::PipelineCompilerVK(const PipelineCompilerVKContext& p_context, std::string_view p_compiler_name)
    : base_context(p_context) {
    if (base_context.alloc == nullptr || base_context.rend_data == nullptr || base_context.rend_cache == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to create {} compiler: invalid context.", p_compiler_name));
    }
}

Ref<ROShaderVK> PipelineCompilerVK::compile_shader(RefRes<RUShader> p_shader) {
    if (p_shader == nullptr || p_shader->resource_key == nullptr) {
        throw std::runtime_error("Failed to compile shader: shader resource key was not propagated.");
    }
    std::string shader_key = p_shader->resource_key;
    return base_context.rend_cache->acquire<ROShaderVK>(shader_key, [this, p_shader] {
        return base_context.rend_data->create_shader(ROShaderVK::Builder().set_code(p_shader));
    });
}

} // namespace WhiteBirdEngine
