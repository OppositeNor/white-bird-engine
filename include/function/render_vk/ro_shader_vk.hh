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
    // Builder for ROShaderVK
    struct Builder {
        friend ROShaderVK;
        // The source code of the shader.
        std::optional<std::vector<char>> code;

        // Set the shader source code.
        Builder& set_code(const std::vector<char>& p_code);
        // Set the shader source code from a file.
        Builder& set_code_from_file(const std::string& p_file_path);

    private:
        VkShaderModule build(const RendererVK::BaseRenderDataVK* p_render_data) const;
    };

    ROShaderVK() = delete;
    ~ROShaderVK();
    ROShaderVK(const ROShaderVK&) = delete;
    ROShaderVK(ROShaderVK&&) = delete;
    ROShaderVK& operator=(const ROShaderVK&) = delete;
    ROShaderVK& operator=(ROShaderVK&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_base_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    ROShaderVK(const RendererVK::BaseRenderDataVK* p_base_render_data, const Builder& p_builder);

    virtual std::string get_ro_type() const {
        return "shader_vk";
    }

    /**
     * @brief Get the shader module.
     *
     * @return The shader module.
     */
    VkShaderModule get_shader_module() const;

private:
    VkShaderModule shader_module;
    const RendererVK::BaseRenderDataVK* render_data;

    void init(const Builder& p_builder);
};

}

#endif
