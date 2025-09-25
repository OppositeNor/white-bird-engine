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
#include "function/render_vk/pipeline_vk.hh"

namespace WhiteBirdEngine {

PipelineVK::PipelineVK(const vector<ShaderPipelineVK>& p_shaders) {
    // TODO
}

void PipelineVK::initialize(const vector<ShaderPipelineVK>& p_shaders) {
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(p_shaders.size());
    for (auto& shader : p_shaders) {
        shader_stages.push_back(shader.get_shader_stage_create_info());
    }
    // TODO
}

}
