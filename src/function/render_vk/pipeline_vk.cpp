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
