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
#ifndef WBE_FILE_SHADER_METADATA_VK_UTILS_HH
#define WBE_FILE_SHADER_METADATA_VK_UTILS_HH

#include "function/render_vk/render_vk_resource_unit.hh"
#include "function/render_vk/shader_metadata_vk.hh"
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ShaderDescriptorBindingVK
 * @brief Vulkan descriptor binding translated from shader reflection metadata.
 */
struct ShaderDescriptorBindingVK {
    /**
     * @brief Shader parameter name, used to match material texture slots.
     */
    std::string parameter_name;
    /**
     * @brief Descriptor set index from the shader binding space.
     */
    uint32_t set_index = 0;
    /**
     * @brief Vulkan layout binding for this parameter.
     */
    VkDescriptorSetLayoutBinding layout_binding{};
};

/**
 * @class ShaderDescriptorSetLayoutVK
 * @brief Descriptor layout bindings grouped by Vulkan descriptor set index.
 */
struct ShaderDescriptorSetLayoutVK {
    /**
     * @brief Descriptor set index.
     */
    uint32_t set_index = 0;
    /**
     * @brief Sorted bindings that belong to this set.
     */
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

/**
 * @brief Find a shader entry point by stage and entry function name.
 */
const ShaderMetadataEntryPoint& find_shader_entry_point(
    const ShaderMetadata& p_metadata, const std::string& p_stage, const std::string& p_entry_name);

/**
 * @brief Convert a reflected shader stage string into a Vulkan stage flag.
 */
VkShaderStageFlagBits shader_stage_flag_from_metadata_stage(const std::string& p_stage);

/**
 * @brief Collect descriptor bindings used by one shader entry point.
 */
std::vector<ShaderDescriptorBindingVK> collect_shader_descriptor_bindings(
    const RUShader& p_shader, const std::string& p_stage, const std::string& p_entry_name);

/**
 * @brief Collect and merge descriptor bindings from all supported graphics stages.
 */
std::vector<ShaderDescriptorBindingVK> collect_graphics_pipeline_descriptor_bindings(const RUGraphicsPipeline& p_pipeline);

/**
 * @brief Build descriptor set layout inputs for a graphics pipeline resource.
 */
std::vector<ShaderDescriptorSetLayoutVK> build_graphics_pipeline_descriptor_set_layout_bindings(const RUGraphicsPipeline& p_pipeline);

} // namespace WhiteBirdEngine

#endif