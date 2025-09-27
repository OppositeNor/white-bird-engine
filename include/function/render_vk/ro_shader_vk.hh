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
#ifndef __WBE_RO_SHADER_VK_HH__
#define __WBE_RO_SHADER_VK_HH__

#include "function/render/render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include <string>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ShaderVK
 * @brief Shader class for Vulkan.
 *
 */
class ROShaderVK final : public RenderObject {
public:

    /**
     * @class Config
     * @brief Shader configuration.
     *
     */
    struct Config {
        // The source code of the shader.
        std::vector<char> code;
    };

    ROShaderVK() = delete;
    ~ROShaderVK();
    ROShaderVK(const ROShaderVK&) = delete;
    ROShaderVK(ROShaderVK&&) = delete;
    ROShaderVK& operator=(const ROShaderVK&) = delete;
    ROShaderVK& operator=(ROShaderVK&&) = delete;

    /**
     * @brief Construct from a chunk of data..
     *
     * @tparam AllocType The type of the allocator for the vector.
     * @param p_shader_data The data of the shader.
     */
    ROShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const Config& p_config);

    virtual std::string get_ro_type() const {
        return "shader_vk";
    }

    /**
     * @brief Get the shader stage create info.
     *
     * @return The shader stage create info.
     */
    VkShaderModule get_shader_module() const;

private:
    VkShaderModule shader_module;
    const RendererVK::BaseRenderDataVK* render_data;

    void init(const Config& p_config);
    VkShaderModule create_shader_module(const Config& p_config);
};

}

#endif
