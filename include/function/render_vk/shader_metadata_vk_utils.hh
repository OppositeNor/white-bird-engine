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

#include "function/render/render_database.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/shader_metadata_vk.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ShaderDescriptorFieldBindingVK
 * @brief Reflected field inside a descriptor-backed shader struct.
 */
struct ShaderDescriptorFieldBindingVK {
    /**
     * @brief Field path relative to the shader parameter, e.g. "camera_info.view".
     */
    std::string field_path;
    /**
     * @brief Byte offset inside the descriptor buffer.
     */
    VkDeviceSize offset = 0;
    /**
     * @brief Byte size of the reflected field.
     */
    VkDeviceSize size = 0;
};

/**
 * @class ShaderDescriptorBindingVK
 * @brief Vulkan descriptor binding translated from shader reflection metadata.
 */
struct ShaderDescriptorBindingVK {
    /**
     * @brief Reflected shader parameter name used to match bindpoint field paths.
     */
    std::string parameter_name;
    /**
     * @brief Reflected shader type name for this descriptor.
     */
    std::string type_name;
    /**
     * @brief Byte size of a descriptor-backed buffer.
     */
    VkDeviceSize buffer_size = 0;
    /**
     * @brief Reflected fields inside a descriptor-backed buffer.
     */
    std::vector<ShaderDescriptorFieldBindingVK> fields;
    /**
     * @brief Descriptor set index from the shader binding space.
     */
    uint32_t set_index = 0;
    /**
     * @brief Vulkan layout binding for this parameter.
     */
    VkDescriptorSetLayoutBinding layout_binding{};
    /**
     * @brief Whether this binding is a runtime array allocated with a variable descriptor count.
     */
    bool variable_descriptor_count = false;
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
    /**
     * @brief Descriptor indexing flags aligned with bindings.
     */
    std::vector<VkDescriptorBindingFlags> binding_flags;
};

/**
 * @brief Convert a reflected shader stage string into a Vulkan stage flag.
 */
VkShaderStageFlagBits shader_stage_flag_from_metadata_stage(std::string_view p_stage);

/**
 * @brief Reflect and match one vertex entry point against a resource-defined vertex layout.
 *
 * Every field marked WBE_VERTEX_INPUT must match an exact resource role and format. Extra resource attributes are allowed.
 * Locations come from the reflected varying-input binding indices. A vertex entry without annotated fields produces an empty
 * vertex-less state.
 *
 * @throws std::runtime_error When attributes, roles, locations, formats, arguments, or resource layout metadata are invalid.
 */
VertexInputDescriptionVK build_vertex_input_description_vk(
    const ShaderMetadata& p_metadata, std::string_view p_entry_name, const RUVertexData& p_vertex_data);

/**
 * @copydoc build_vertex_input_description_vk(const ShaderMetadata&, std::string_view, const RUVertexData&)
 */
VertexInputDescriptionVK build_vertex_input_description_vk(const ShaderMetadata& p_metadata,
    std::string_view p_entry_name,
    const RenderDatabase::OpaqueTrianglesInfo::VertexLayout& p_vertex_layout);

/**
 * @brief Collect descriptor bindings used by one shader entry point.
 */
std::vector<ShaderDescriptorBindingVK> collect_shader_descriptor_bindings(
    const ShaderMetadata& p_metadata, std::string_view p_stage, std::string_view p_entry_name);

/**
 * @brief Collect and merge descriptor bindings from all supported graphics stages.
 */
std::vector<ShaderDescriptorBindingVK> collect_graphics_pipeline_descriptor_bindings(const ShaderMetadata& p_vertex_shader_metadata,
    std::string_view p_vertex_stage_entry,
    const ShaderMetadata* p_geometry_shader_metadata,
    std::string_view p_geometry_stage_entry,
    const ShaderMetadata& p_fragment_shader_metadata,
    std::string_view p_fragment_stage_entry);

/**
 * @brief Build descriptor set layout inputs for a graphics pipeline resource.
 */
std::vector<ShaderDescriptorSetLayoutVK> build_graphics_pipeline_descriptor_set_layout_bindings(
    const std::vector<ShaderDescriptorBindingVK>& p_bindings);

/**
 * @brief Build descriptor set layout inputs for any reflected pipeline resource.
 */
std::vector<ShaderDescriptorSetLayoutVK> build_shader_descriptor_set_layout_bindings(
    const std::vector<ShaderDescriptorBindingVK>& p_bindings);

/**
 * @brief Assign a stable upper bound shared by all variable-count bindings in the pipeline layouts.
 *
 * Fixed descriptors are reserved first. The remaining descriptor budget is divided between variable bindings so the declared
 * layout maxima remain within the supplied device/engine resource cap.
 *
 * @param p_set_layouts Descriptor set layouts to update.
 * @param p_max_descriptor_count Maximum descriptor resource budget available to the pipeline.
 */
void set_variable_descriptor_count_upper_bounds(std::vector<ShaderDescriptorSetLayoutVK>& p_set_layouts, uint32_t p_max_descriptor_count);

} // namespace WhiteBirdEngine

#endif
