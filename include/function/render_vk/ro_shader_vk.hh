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
#ifndef WBE_FILE_RO_SHADER_VK_HH
#define WBE_FILE_RO_SHADER_VK_HH

#include "core/reflection/reflection_defs.hh"
#include "core/memory/unique.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/utils.hh"
#include <cstddef>
#include <string>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ShaderVK
 * @brief Shader class for Vulkan.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROShaderVK final : public RenderObjectVK {
public:
    // Builder for ROShaderVK
    struct Builder {
        friend ROShaderVK;
        // The source code of the shader.
        Required<const char*> code;
        // Size of the code.
        Required<size_t> code_size;

        // Set the shader source code.
        Builder& set_code(const char* p_code, size_t p_code_size);

        // Set from a resource ID.
        Builder& set_code(const std::string& p_resource_id);

        // Set from a resource.
        Builder& set_code(RefRes<RUShader> p_resource);

        Ref<ROShaderVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROShaderVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<ROShaderVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROShaderVK>(p_render_data->allocator, p_render_data, *this);
        }
    private:
        VkShaderModule create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };

    ROShaderVK() = delete;
    virtual ~ROShaderVK() override;
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
    ROShaderVK(const BaseRenderDataVK* p_base_render_data, const Builder& p_builder);
    /**
     * @brief Constructor from resource ID.
     *
     * @param p_base_render_data The base render data.
     * @param p_resource_id The resource ID.
     */
    ROShaderVK(const BaseRenderDataVK* p_base_render_data, const std::string& p_resource_id);
    /**
     * @brief Constructor from a shader resource.
     *
     * @param p_base_render_data The base render data.
     * @param p_resource_id The shader's resource.
     */
    ROShaderVK(const BaseRenderDataVK* p_base_render_data, RefRes<RUShader> p_resource);

    /**
     * @brief Get the shader module.
     *
     * @return The shader module.
     */
    VkShaderModule get_shader_module() const;

private:
    VkShaderModule shader_module = VK_NULL_HANDLE;

    void init(const Builder& p_builder);
};
} // namespace WhiteBirdEngine

#endif
