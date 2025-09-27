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
#ifndef __WBE_SHADER_VK_HH__
#define __WBE_SHADER_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include "platform/file_system/path.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include <string>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ShaderVK
 * @brief Shader class for Vulkan.
 *
 */
class ShaderVK final : public RenderObject {
public:

    struct ShaderConfigurations {
        // Shader stages bits.
        VkShaderStageFlagBits stages;
        // The entry function name of the shader.
        std::string entry_function = "main";
    };

    ShaderVK() = delete;
    ~ShaderVK();
    ShaderVK(const ShaderVK&) = delete;
    ShaderVK(ShaderVK&&) = delete;
    ShaderVK& operator=(const ShaderVK&) = delete;
    ShaderVK& operator=(ShaderVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_shader_res_id The resource ID referencing to the source of the shader.
     */
    ShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const ShaderConfigurations* p_configurations,
                     ResourceChunkID p_resource_chunk, const std::string& p_shader_res_id);

    /**
     * @brief Constructor.
     *
     * @param p_shader_file_path The path to the source of the shader.
     */
    ShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const ShaderConfigurations* p_configurations,
                     const Path& p_shader_file_path);

    /**
     * @brief Construct from a chunk of data..
     *
     * @tparam AllocType The type of the allocator for the vector.
     * @param p_shader_data The data of the shader.
     */
    ShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const ShaderConfigurations* p_configurations,
                     const char* p_code_data, size_t p_data_size);

    virtual std::string get_ro_type() const {
        return "shader_vk";
    }

    /**
     * @brief Get the shader stage create info.
     *
     * @return The shader stage create info.
     */
    VkPipelineShaderStageCreateInfo get_shader_stage_create_info() const;

private:
    VkShaderModule shader_module;

    void initialize_shader(const char* p_code_data, size_t p_code_size);
    const RendererVK::BaseRenderDataVK* base_render_data;
    const ShaderConfigurations* configurations;
    VkShaderModule create_shader_module(const RendererVK::BaseRenderDataVK* p_base_render_data, const char* p_code_data, size_t p_code_size);
};

}

#endif
