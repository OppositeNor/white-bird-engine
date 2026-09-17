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
#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @brief Resource category selected by a reflected WBE_DESCRIPTOR annotation.
 */
enum class ShaderDescriptorHeapTypeVK : uint8_t {
    UNIFORM_BUFFER,
    STORAGE_BUFFER,
    SAMPLED_IMAGE,
    STORAGE_IMAGE,
    SAMPLER,
    ACCELERATION_STRUCTURE,
};

/**
 * @class ShaderDescriptorHeapFieldVK
 * @brief One descriptor-heap index stored in reflected pipeline push data.
 */
struct ShaderDescriptorHeapFieldVK {
    /** Path relative to the push-data parameter. */
    std::string field_path;
    /** Descriptor heap category required by this field. */
    ShaderDescriptorHeapTypeVK type = ShaderDescriptorHeapTypeVK::UNIFORM_BUFFER;
    /** Byte offset of the explicit uint2 inside push data. */
    uint32_t offset = 0;
};

/**
 * @class ShaderDescriptorHeapLayoutVK
 * @brief Reflected descriptor-index push-data contract for one pipeline.
 */
struct ShaderDescriptorHeapLayoutVK {
    /** Total byte size required by the reflected push-data block. */
    uint32_t size = 0;
    /** Shader stages which consume this push-data contract. */
    VkShaderStageFlags stage_flags = 0;
    /** Descriptor-index fields in declaration order. */
    std::vector<ShaderDescriptorHeapFieldVK> fields;
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
    const ShaderMetadata& p_metadata, std::string_view p_entry_name, const RUVertexView& p_vertex_data);

/**
 * @copydoc build_vertex_input_description_vk(const ShaderMetadata&, std::string_view, const RUVertexView&)
 */
VertexInputDescriptionVK build_vertex_input_description_vk(const ShaderMetadata& p_metadata,
    std::string_view p_entry_name,
    const RenderDatabase::OpaqueTrianglesInfo::VertexLayout& p_vertex_layout);

/**
 * @brief Collect the explicit uint2 descriptor-heap push-data contract used by one shader entry point.
 *
 * A descriptor-free entry point produces an empty layout. Otherwise it may use exactly one pushConstantBuffer parameter.
 * Annotated fields must be eight-byte uint2 values with one supported WBE_DESCRIPTOR string argument.
 *
 * @throws std::runtime_error When the reflected push-data or annotation contract is malformed.
 */
ShaderDescriptorHeapLayoutVK collect_shader_descriptor_heap_layout(
    const ShaderMetadata& p_metadata, std::string_view p_stage, std::string_view p_entry_name);

/**
 * @brief Collect and merge explicit descriptor-heap push-data contracts from all supported graphics stages.
 *
 * Equal field paths must agree on descriptor category and byte offset. Distinct fields must not overlap in push data.
 *
 * @throws std::runtime_error When stage contracts conflict.
 */
ShaderDescriptorHeapLayoutVK collect_graphics_pipeline_descriptor_heap_layout(const ShaderMetadata& p_vertex_shader_metadata,
    std::string_view p_vertex_stage_entry,
    const ShaderMetadata* p_geometry_shader_metadata,
    std::string_view p_geometry_stage_entry,
    const ShaderMetadata& p_fragment_shader_metadata,
    std::string_view p_fragment_stage_entry);

} // namespace WhiteBirdEngine

#endif
