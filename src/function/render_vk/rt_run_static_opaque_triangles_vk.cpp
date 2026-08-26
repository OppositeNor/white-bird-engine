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
#include "function/render_vk/rt_run_static_opaque_triangles_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "core/memory/unique.hh"
#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/material_texture_contract_resolver.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render/shader_role.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/static_opaque_vertex_batch_planner_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_default_material_resource_key(std::string_view p_resource_key) {
    default_material_resource_key = p_resource_key;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_material_textures_variable_name(
    std::string_view p_variable_name) {
    material_textures_variable_name = p_variable_name;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_material_params_variable_name(
    std::string_view p_variable_name) {
    material_params_variable_name = p_variable_name;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_graphics_pipeline_resource_key(
    std::string_view p_resource_key) {
    graphics_pipeline_resource_key = p_resource_key;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::add_graphics_pipeline_override(
    std::string_view p_source_resource_key, std::string_view p_target_resource_key) {
    PipelineOverride pipeline_override;
    pipeline_override.source_resource_key = p_source_resource_key;
    pipeline_override.target_resource_key = p_target_resource_key;
    graphics_pipeline_overrides.push_back(std::move(pipeline_override));
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_render_extent(VkExtent2D p_render_extent) {
    render_extent = p_render_extent;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::add_color_attachment(
    std::string_view p_resource_id, VkFormat p_format) {
    color_attachment_ids.emplace_back(p_resource_id);
    color_attachment_formats.push_back(p_format);
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_depth_attachment(
    std::string_view p_resource_id, VkFormat p_format) {
    depth_attachment_id = p_resource_id;
    depth_attachment_format = p_format;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_stencil_attachment_format(VkFormat p_format) {
    stencil_attachment_format = p_format;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::add_clear_value(VkClearValue p_clear_value) {
    clear_values.push_back(p_clear_value);
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_color_attachment_load_op(VkAttachmentLoadOp p_load_op) {
    color_attachment_load_op = p_load_op;
    return *this;
}

RTRunStaticOpaqueTrianglesVK::Config& RTRunStaticOpaqueTrianglesVK::Config::set_depth_attachment_load_op(VkAttachmentLoadOp p_load_op) {
    depth_attachment_load_op = p_load_op;
    return *this;
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_default_material_resource_key() const {
    return default_material_resource_key;
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_material_textures_variable_name() const {
    return material_textures_variable_name;
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_material_params_variable_name() const {
    return material_params_variable_name;
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_graphics_pipeline_resource_key() const {
    return graphics_pipeline_resource_key;
}

size_t RTRunStaticOpaqueTrianglesVK::Config::get_graphics_pipeline_override_count() const {
    return graphics_pipeline_overrides.size();
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_graphics_pipeline_override_source(size_t p_index) const {
    if (p_index >= graphics_pipeline_overrides.size()) {
        throw std::runtime_error("Failed to read static opaque task configuration: pipeline override index is out of bounds.");
    }
    return graphics_pipeline_overrides[p_index].source_resource_key;
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_graphics_pipeline_override_target(size_t p_index) const {
    if (p_index >= graphics_pipeline_overrides.size()) {
        throw std::runtime_error("Failed to read static opaque task configuration: pipeline override index is out of bounds.");
    }
    return graphics_pipeline_overrides[p_index].target_resource_key;
}

VkExtent2D RTRunStaticOpaqueTrianglesVK::Config::get_render_extent() const {
    return render_extent;
}

size_t RTRunStaticOpaqueTrianglesVK::Config::get_color_attachment_count() const {
    return color_attachment_ids.size();
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_color_attachment_id(size_t p_index) const {
    if (p_index >= color_attachment_ids.size()) {
        throw std::runtime_error("Failed to read static opaque task configuration: color attachment index is out of bounds.");
    }
    return color_attachment_ids[p_index];
}

VkFormat RTRunStaticOpaqueTrianglesVK::Config::get_color_attachment_format(size_t p_index) const {
    if (p_index >= color_attachment_formats.size()) {
        throw std::runtime_error("Failed to read static opaque task configuration: color attachment index is out of bounds.");
    }
    return color_attachment_formats[p_index];
}

bool RTRunStaticOpaqueTrianglesVK::Config::has_depth_attachment() const {
    return depth_attachment_id.has_value();
}

std::string_view RTRunStaticOpaqueTrianglesVK::Config::get_depth_attachment_id() const {
    if (!depth_attachment_id.has_value()) {
        throw std::runtime_error("Failed to read static opaque task configuration: no depth attachment is configured.");
    }
    return *depth_attachment_id;
}

VkFormat RTRunStaticOpaqueTrianglesVK::Config::get_depth_attachment_format() const {
    return depth_attachment_format;
}

VkFormat RTRunStaticOpaqueTrianglesVK::Config::get_stencil_attachment_format() const {
    return stencil_attachment_format;
}

size_t RTRunStaticOpaqueTrianglesVK::Config::get_clear_value_count() const {
    return clear_values.size();
}

VkClearValue RTRunStaticOpaqueTrianglesVK::Config::get_clear_value(size_t p_index) const {
    if (p_index >= clear_values.size()) {
        throw std::runtime_error("Failed to read static opaque task configuration: clear value index is out of bounds.");
    }
    return clear_values[p_index];
}

VkAttachmentLoadOp RTRunStaticOpaqueTrianglesVK::Config::get_color_attachment_load_op() const {
    return color_attachment_load_op;
}

VkAttachmentLoadOp RTRunStaticOpaqueTrianglesVK::Config::get_depth_attachment_load_op() const {
    return depth_attachment_load_op;
}

struct StaticOpaqueDrawDataVK {
    struct PipelineBatch {
        PipelineBatch() = default;
        ~PipelineBatch() = default;
        PipelineBatch(const PipelineBatch&) = default;
        PipelineBatch(PipelineBatch&&) noexcept = default;
        PipelineBatch& operator=(const PipelineBatch&) = default;
        PipelineBatch& operator=(PipelineBatch&&) noexcept = default;

        RefRes<RUGraphicsPipeline> graphics_pipeline = MEM_NULL;
        std::string graphics_pipeline_key;
        uint32_t first_draw = 0;
        uint32_t draw_count = 0;
    };

    explicit StaticOpaqueDrawDataVK(RenderAllocator* p_allocator)
        : indirect_commands(p_allocator), pipeline_batches(p_allocator) {
    }
    ~StaticOpaqueDrawDataVK() = default;
    StaticOpaqueDrawDataVK(const StaticOpaqueDrawDataVK&) = delete;
    StaticOpaqueDrawDataVK(StaticOpaqueDrawDataVK&&) noexcept = default;
    StaticOpaqueDrawDataVK& operator=(const StaticOpaqueDrawDataVK&) = delete;
    StaticOpaqueDrawDataVK& operator=(StaticOpaqueDrawDataVK&&) noexcept = default;

    Vector<VkDrawIndexedIndirectCommand, RenderAllocator> indirect_commands;
    Vector<PipelineBatch, RenderAllocator> pipeline_batches;
};

struct StaticOpaqueMaterialDataVK {
    StaticOpaqueMaterialDataVK() = default;
    ~StaticOpaqueMaterialDataVK() = default;
    StaticOpaqueMaterialDataVK(const StaticOpaqueMaterialDataVK&) = default;
    StaticOpaqueMaterialDataVK(StaticOpaqueMaterialDataVK&&) noexcept = default;
    StaticOpaqueMaterialDataVK& operator=(const StaticOpaqueMaterialDataVK&) = default;
    StaticOpaqueMaterialDataVK& operator=(StaticOpaqueMaterialDataVK&&) noexcept = default;

    RenderBindpointDeclarations bindpoints;
    RefRes<RUImage> default_image = MEM_NULL;
};

struct StaticOpaqueAttachmentDataVK {
    StaticOpaqueAttachmentDataVK() = default;
    ~StaticOpaqueAttachmentDataVK() = default;
    StaticOpaqueAttachmentDataVK(const StaticOpaqueAttachmentDataVK&) = default;
    StaticOpaqueAttachmentDataVK(StaticOpaqueAttachmentDataVK&&) noexcept = default;
    StaticOpaqueAttachmentDataVK& operator=(const StaticOpaqueAttachmentDataVK&) = default;
    StaticOpaqueAttachmentDataVK& operator=(StaticOpaqueAttachmentDataVK&&) noexcept = default;

    VkExtent2D render_extent{};
    std::vector<std::string> color_attachment_ids;
    std::vector<VkFormat> color_attachment_formats;
    std::vector<VkClearValue> clear_values;
};

struct StaticOpaqueCompiledPipelineBatchVK {
    StaticOpaqueCompiledPipelineBatchVK() = default;
    ~StaticOpaqueCompiledPipelineBatchVK() = default;
    StaticOpaqueCompiledPipelineBatchVK(const StaticOpaqueCompiledPipelineBatchVK&) = default;
    StaticOpaqueCompiledPipelineBatchVK(StaticOpaqueCompiledPipelineBatchVK&&) noexcept = default;
    StaticOpaqueCompiledPipelineBatchVK& operator=(const StaticOpaqueCompiledPipelineBatchVK&) = default;
    StaticOpaqueCompiledPipelineBatchVK& operator=(StaticOpaqueCompiledPipelineBatchVK&&) noexcept = default;

    std::string resource_key;
    GraphicsPipelineCompilerVK::CompileResult compiled;
    uint32_t first_draw = 0;
    uint32_t draw_count = 0;
};

namespace {
const BaseRenderDataVK* get_render_data(RendererVK* p_renderer) {
    if (p_renderer == nullptr || p_renderer->get_base_render_data() == nullptr || p_renderer->get_base_render_data()->allocator == nullptr) {
        throw std::runtime_error("Failed to construct static opaque task: renderer state is missing.");
    }
    return p_renderer->get_base_render_data();
}

void validate_task_config(const RTRunStaticOpaqueTrianglesVK::Config& p_config) {
    if (p_config.get_default_material_resource_key().empty() || p_config.get_material_textures_variable_name().empty() ||
        p_config.get_material_params_variable_name().empty()) {
        throw std::runtime_error("Failed to construct static opaque task: material contract base names must not be empty.");
    }
}

std::optional<MaterialTextureContract> reflect_pipeline_material_texture_contract(RefRes<RUGraphicsPipeline> p_pipeline,
    std::string_view p_material_textures_variable_name,
    std::string_view p_material_params_variable_name) {
    if (p_pipeline == nullptr || p_pipeline->vertex_stage == nullptr || p_pipeline->fragment_stage == nullptr) {
        throw std::runtime_error("Failed to reflect static opaque material contract: graphics pipeline stages are missing.");
    }
    ShaderMetadata vertex_metadata = ShaderMetadataLoader().load(p_pipeline->vertex_stage);
    ShaderMetadata fragment_metadata = ShaderMetadataLoader().load(p_pipeline->fragment_stage);
    std::optional<ShaderMetadata> geometry_metadata;
    if (p_pipeline->geometry_stage != nullptr) {
        geometry_metadata = ShaderMetadataLoader().load(p_pipeline->geometry_stage);
    }
    return MaterialTextureContractResolver().resolve(vertex_metadata,
        geometry_metadata.has_value() ? &*geometry_metadata : nullptr,
        fragment_metadata,
        p_material_textures_variable_name,
        p_material_params_variable_name);
}

RefRes<RUImage> find_material_texture(RefRes<RUMaterial> p_material, const ShaderRole& p_texture_role) {
    if (p_material == nullptr || (p_material->texture_count > 0 && p_material->textures == nullptr)) {
        throw std::runtime_error("Failed to construct static opaque task: material texture storage is missing.");
    }
    RefRes<RUImage> result = MEM_NULL;
    for (size_t i = 0; i < p_material->texture_count; ++i) {
        const RUMaterial::TextureSlot& texture_slot = p_material->textures.get()[i];
        if (!texture_slot.role.matches(p_texture_role.get_name())) {
            continue;
        }
        if (result != nullptr) {
            throw std::runtime_error("Failed to construct static opaque task: material contains a duplicate texture role.");
        }
        result = texture_slot.texture;
    }
    return result;
}

RefRes<RUImage> find_first_material_texture(RefRes<RUMaterial> p_material) {
    if (p_material == nullptr || p_material->texture_count == 0 || p_material->textures == nullptr) {
        return MEM_NULL;
    }
    return p_material->textures.get()[0].texture;
}

void write_material_texture_index(
    std::vector<uint8_t>& p_buffer, const MaterialTextureContract& p_contract, size_t p_material_index, size_t p_field_offset, int32_t p_texture_index) {
    size_t offset = p_contract.table_offset + p_material_index * p_contract.table_stride + p_field_offset;
    if (offset > p_buffer.size() || sizeof(p_texture_index) > p_buffer.size() - offset) {
        throw std::runtime_error("Failed to construct static opaque task: reflected material index write is out of bounds.");
    }
    std::memcpy(p_buffer.data() + offset, &p_texture_index, sizeof(p_texture_index));
}

void validate_draw_data_source(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info) {
    if (p_triangles_info.vertices.empty() || p_triangles_info.indices.empty() || p_triangles_info.materials.empty() ||
        p_triangles_info.draw_groups.empty() || p_triangles_info.graphics_pipelines.empty()) {
        throw std::runtime_error("Failed to construct static opaque task: the render database has no drawable triangles.");
    }
}

void append_indirect_commands(StaticOpaqueDrawDataVK& p_draw_data, const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info) {
    p_draw_data.indirect_commands.reserve(p_triangles_info.draw_groups.size());
    for (const auto& draw_group : p_triangles_info.draw_groups) {
        if (draw_group.index_sub_region.starting_index > p_triangles_info.indices.size() ||
            draw_group.index_sub_region.size > p_triangles_info.indices.size() - draw_group.index_sub_region.starting_index ||
            draw_group.index_sub_region.starting_index > std::numeric_limits<uint32_t>::max() ||
            draw_group.index_sub_region.size > std::numeric_limits<uint32_t>::max() ||
            draw_group.material_index >= p_triangles_info.materials.size() ||
            draw_group.material_index >= std::numeric_limits<uint32_t>::max() ||
            draw_group.indirect_command_sub_region.starting_index != p_draw_data.indirect_commands.size() ||
            draw_group.indirect_command_sub_region.size != 1) {
            throw std::runtime_error("Failed to construct static opaque task: a draw-group range is invalid.");
        }
        p_draw_data.indirect_commands.push_back(VkDrawIndexedIndirectCommand{
            .indexCount = static_cast<uint32_t>(draw_group.index_sub_region.size),
            .instanceCount = 1,
            .firstIndex = static_cast<uint32_t>(draw_group.index_sub_region.starting_index),
            .vertexOffset = 0,
            .firstInstance = static_cast<uint32_t>(draw_group.material_index) + 1,
        });
    }
}

void validate_pipeline_overrides(const RTRunStaticOpaqueTrianglesVK::Config& p_config) {
    for (size_t i = 0; i < p_config.get_graphics_pipeline_override_count(); ++i) {
        if (p_config.get_graphics_pipeline_override_source(i).empty() || p_config.get_graphics_pipeline_override_target(i).empty()) {
            throw std::runtime_error("Failed to construct static opaque task: pipeline override keys must not be empty.");
        }
    }
}

std::string resolve_target_pipeline_key(std::string_view p_source_pipeline_key, const RTRunStaticOpaqueTrianglesVK::Config& p_config) {
    if (!p_config.get_graphics_pipeline_resource_key().empty()) {
        return std::string(p_config.get_graphics_pipeline_resource_key());
    }
    for (size_t i = 0; i < p_config.get_graphics_pipeline_override_count(); ++i) {
        if (p_config.get_graphics_pipeline_override_source(i) == p_source_pipeline_key) {
            return std::string(p_config.get_graphics_pipeline_override_target(i));
        }
    }
    return std::string(p_source_pipeline_key);
}

void append_pipeline_batch(StaticOpaqueDrawDataVK& p_draw_data,
    RefRes<RUGraphicsPipeline> p_graphics_pipeline,
    std::string p_graphics_pipeline_key,
    uint32_t p_first_draw,
    uint32_t p_draw_count) {
    if (!p_draw_data.pipeline_batches.empty() && p_draw_data.pipeline_batches.back().graphics_pipeline_key == p_graphics_pipeline_key &&
        p_draw_data.pipeline_batches.back().first_draw + p_draw_data.pipeline_batches.back().draw_count == p_first_draw) {
        p_draw_data.pipeline_batches.back().draw_count += p_draw_count;
        return;
    }
    StaticOpaqueDrawDataVK::PipelineBatch pipeline_batch;
    pipeline_batch.graphics_pipeline = p_graphics_pipeline;
    pipeline_batch.graphics_pipeline_key = std::move(p_graphics_pipeline_key);
    pipeline_batch.first_draw = p_first_draw;
    pipeline_batch.draw_count = p_draw_count;
    p_draw_data.pipeline_batches.push_back(std::move(pipeline_batch));
}

void append_pipeline_batches(StaticOpaqueDrawDataVK& p_draw_data,
    const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const RTRunStaticOpaqueTrianglesVK::Config& p_config) {
    auto* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr) {
        throw std::runtime_error("Failed to construct static opaque task: resource manager is missing.");
    }
    validate_pipeline_overrides(p_config);
    for (const auto& pipeline_unit : p_triangles_info.graphics_pipelines) {
        if (pipeline_unit.graphics_pipeline == nullptr || pipeline_unit.graphics_pipeline->resource_key == nullptr) {
            throw std::runtime_error("Failed to construct static opaque task: a material graphics pipeline is missing.");
        }
        if (pipeline_unit.draw_group_sub_region.starting_index > p_triangles_info.draw_groups.size() ||
            pipeline_unit.draw_group_sub_region.size >
                p_triangles_info.draw_groups.size() - pipeline_unit.draw_group_sub_region.starting_index ||
            pipeline_unit.draw_group_sub_region.starting_index > std::numeric_limits<uint32_t>::max() ||
            pipeline_unit.draw_group_sub_region.size > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to construct static opaque task: a graphics-pipeline draw-group range is invalid.");
        }
        std::string source_pipeline_key = pipeline_unit.graphics_pipeline->resource_key;
        std::string target_pipeline_key = resolve_target_pipeline_key(source_pipeline_key, p_config);
        RefRes<RUGraphicsPipeline> target_pipeline = target_pipeline_key == source_pipeline_key
                                                         ? pipeline_unit.graphics_pipeline
                                                         : resource_manager->get_resource<RUGraphicsPipeline>(target_pipeline_key);
        if (target_pipeline == nullptr) {
            throw std::runtime_error(
                std::format("Failed to construct static opaque task: graphics pipeline '{}' was not found.", target_pipeline_key));
        }
        uint32_t first_draw = static_cast<uint32_t>(pipeline_unit.draw_group_sub_region.starting_index);
        uint32_t draw_count = static_cast<uint32_t>(pipeline_unit.draw_group_sub_region.size);
        append_pipeline_batch(p_draw_data, target_pipeline, std::move(target_pipeline_key), first_draw, draw_count);
    }
}

StaticOpaqueDrawDataVK build_draw_data(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const RTRunStaticOpaqueTrianglesVK::Config& p_config,
    RenderAllocator* p_allocator) {
    validate_draw_data_source(p_triangles_info);
    StaticOpaqueDrawDataVK result(p_allocator);
    append_indirect_commands(result, p_triangles_info);
    append_pipeline_batches(result, p_triangles_info, p_config);
    return result;
}

void apply_vertex_offsets(StaticOpaqueDrawDataVK& p_draw_data,
    const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueVertexBatchPlannerVK::Batch& p_vertex_batch) {
    if (p_vertex_batch.first_draw > p_triangles_info.draw_groups.size() ||
        p_vertex_batch.draw_count > p_triangles_info.draw_groups.size() - p_vertex_batch.first_draw ||
        p_vertex_batch.first_draw > p_draw_data.indirect_commands.size() ||
        p_vertex_batch.draw_count > p_draw_data.indirect_commands.size() - p_vertex_batch.first_draw) {
        throw std::runtime_error("Failed to construct static opaque task: vertex batch range is invalid.");
    }
    const auto& first_draw_group = p_triangles_info.draw_groups[p_vertex_batch.first_draw];
    const size_t vertex_stride = first_draw_group.vertex_layout.vertex_stride;
    if (vertex_stride == 0 || p_vertex_batch.vertex_buffer_offset > first_draw_group.vertex_buffer_byte_offset) {
        throw std::runtime_error("Failed to construct static opaque task: vertex batch stride or offset is invalid.");
    }
    for (uint32_t draw_index = 0; draw_index < p_vertex_batch.draw_count; ++draw_index) {
        const size_t draw_group_index = p_vertex_batch.first_draw + draw_index;
        const auto& draw_group = p_triangles_info.draw_groups[draw_group_index];
        const size_t byte_delta = draw_group.vertex_buffer_byte_offset - p_vertex_batch.vertex_buffer_offset;
        if (byte_delta % vertex_stride != 0) {
            throw std::runtime_error("Failed to construct static opaque task: vertex batch offsets are not stride aligned.");
        }
        const size_t vertex_offset = byte_delta / vertex_stride;
        if (vertex_offset > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            throw std::runtime_error("Failed to construct static opaque task: vertex offset exceeds Vulkan's int32 range.");
        }
        p_draw_data.indirect_commands[draw_group_index].vertexOffset = static_cast<int32_t>(vertex_offset);
    }
}

void validate_material_data_range(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueDrawDataVK::PipelineBatch& p_draw_batch,
    const MaterialTextureContract& p_contract) {
    if (p_draw_batch.first_draw > p_triangles_info.draw_groups.size() ||
        p_draw_batch.draw_count > p_triangles_info.draw_groups.size() - p_draw_batch.first_draw ||
        p_triangles_info.materials.size() + 1 > p_contract.material_count) {
        throw std::runtime_error("Failed to construct static opaque task: pipeline draw or material range exceeds the reflected table.");
    }
    if (p_contract.table_offset > std::numeric_limits<size_t>::max() - p_contract.table_size) {
        throw std::runtime_error("Failed to construct static opaque task: reflected material table size overflows.");
    }
}

int32_t append_material_texture_declaration(RenderBindpointDeclarationsBuilder& p_bindpoint_builder,
    std::string_view p_material_textures_variable_name,
    RefRes<RUImage> p_texture,
    const BaseRenderDataVK* p_render_data) {
    size_t texture_index = p_bindpoint_builder.declerations.image_declerations.size();
    if (texture_index >= p_render_data->max_variable_sampled_image_descriptor_count ||
        texture_index > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        throw std::runtime_error("Failed to construct static opaque task: material texture descriptors exceed the device limit.");
    }
    p_bindpoint_builder.add_image_decleration(std::format("{}.textures[{}]", p_material_textures_variable_name, texture_index), p_texture);
    return static_cast<int32_t>(texture_index);
}

std::vector<int32_t> append_default_material_textures(RenderBindpointDeclarationsBuilder& p_bindpoint_builder,
    const MaterialTextureContract& p_contract,
    const RTRunStaticOpaqueTrianglesVK::Config& p_config,
    const BaseRenderDataVK* p_render_data,
    RefRes<RUMaterial> p_default_material,
    RefRes<RUImage>& p_default_image) {
    std::vector<int32_t> result;
    result.reserve(p_contract.texture_fields.size());
    for (const MaterialTextureContract::TextureField& texture_field : p_contract.texture_fields) {
        RefRes<RUImage> texture = find_material_texture(p_default_material, texture_field.role);
        if (texture == nullptr) {
            throw std::runtime_error(std::format("Failed to construct static opaque task: default material '{}' is missing texture role '{}'.",
                p_config.get_default_material_resource_key(),
                texture_field.role.get_name()));
        }
        result.push_back(append_material_texture_declaration(
            p_bindpoint_builder, p_config.get_material_textures_variable_name(), texture, p_render_data));
        if (p_default_image == nullptr) {
            p_default_image = texture;
        }
    }
    return result;
}

void initialize_material_texture_indices(std::vector<uint8_t>& p_material_params,
    const MaterialTextureContract& p_contract,
    const std::vector<int32_t>& p_default_texture_indices) {
    for (size_t material_index = 0; material_index < p_contract.material_count; ++material_index) {
        for (size_t texture_field_index = 0; texture_field_index < p_contract.texture_fields.size(); ++texture_field_index) {
            write_material_texture_index(p_material_params,
                p_contract,
                material_index,
                p_contract.texture_fields[texture_field_index].offset,
                p_default_texture_indices[texture_field_index]);
        }
    }
}

void append_material_texture_overrides(RenderBindpointDeclarationsBuilder& p_bindpoint_builder,
    std::vector<uint8_t>& p_material_params,
    const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueDrawDataVK::PipelineBatch& p_draw_batch,
    const MaterialTextureContract& p_contract,
    std::string_view p_material_textures_variable_name,
    const BaseRenderDataVK* p_render_data) {
    size_t draw_end = static_cast<size_t>(p_draw_batch.first_draw) + p_draw_batch.draw_count;
    std::vector<bool> processed_materials(p_triangles_info.materials.size(), false);
    for (size_t draw_index = p_draw_batch.first_draw; draw_index < draw_end; ++draw_index) {
        const size_t material_index = p_triangles_info.draw_groups[draw_index].material_index;
        if (material_index >= p_triangles_info.materials.size()) {
            throw std::runtime_error("Failed to construct static opaque task: draw group references an invalid material.");
        }
        if (processed_materials[material_index]) {
            continue;
        }
        processed_materials[material_index] = true;
        RefRes<RUMaterial> material = p_triangles_info.materials[material_index].material;
        for (const MaterialTextureContract::TextureField& texture_field : p_contract.texture_fields) {
            RefRes<RUImage> texture = find_material_texture(material, texture_field.role);
            if (texture == nullptr) {
                continue;
            }
            int32_t texture_index =
                append_material_texture_declaration(p_bindpoint_builder, p_material_textures_variable_name, texture, p_render_data);
            write_material_texture_index(p_material_params, p_contract, material_index + 1, texture_field.offset, texture_index);
        }
    }
}

StaticOpaqueMaterialDataVK build_material_data(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueDrawDataVK::PipelineBatch& p_draw_batch,
    const RTRunStaticOpaqueTrianglesVK::Config& p_config,
    const BaseRenderDataVK* p_render_data,
    RefRes<RUMaterial> p_default_material,
    Ref<IRenderObject> p_material_sampler) {
    StaticOpaqueMaterialDataVK result;
    result.default_image = find_first_material_texture(p_default_material);
    std::optional<MaterialTextureContract> contract = reflect_pipeline_material_texture_contract(
        p_draw_batch.graphics_pipeline, p_config.get_material_textures_variable_name(), p_config.get_material_params_variable_name());
    if (!contract.has_value()) {
        return result;
    }
    validate_material_data_range(p_triangles_info, p_draw_batch, *contract);
    RenderBindpointDeclarationsBuilder bindpoint_builder;
    bindpoint_builder.add_sampler_decleration(
        std::format("{}.material_sampler", p_config.get_material_textures_variable_name()), std::move(p_material_sampler));
    std::vector<uint8_t> material_params(contract->table_offset + contract->table_size, 0);
    std::vector<int32_t> default_texture_indices = append_default_material_textures(
        bindpoint_builder, *contract, p_config, p_render_data, p_default_material, result.default_image);
    initialize_material_texture_indices(material_params, *contract, default_texture_indices);
    append_material_texture_overrides(
        bindpoint_builder, material_params, p_triangles_info, p_draw_batch, *contract, p_config.get_material_textures_variable_name(), p_render_data);
    bindpoint_builder.add_buffer_decleration(
        p_config.get_material_params_variable_name(), material_params.data(), material_params.size());
    result.bindpoints = bindpoint_builder.build();
    return result;
}

RefRes<RUMaterial> get_default_material(const RTRunStaticOpaqueTrianglesVK::Config& p_config) {
    auto* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr) {
        throw std::runtime_error("Failed to construct static opaque task: resource manager is missing.");
    }
    RefRes<RUMaterial> result = resource_manager->get_resource<RUMaterial>(p_config.get_default_material_resource_key());
    if (result == nullptr) {
        throw std::runtime_error(std::format("Failed to construct static opaque task: default material '{}' was not found.",
            p_config.get_default_material_resource_key()));
    }
    return result;
}

void validate_indirect_draw_count(const StaticOpaqueDrawDataVK& p_draw_data, const BaseRenderDataVK* p_render_data) {
    if (p_draw_data.indirect_commands.size() > p_render_data->max_draw_indirect_count) {
        throw std::runtime_error(std::format("Failed to construct static opaque task: {} draws exceed the device limit of {}.",
            p_draw_data.indirect_commands.size(),
            p_render_data->max_draw_indirect_count));
    }
}

StaticOpaqueAttachmentDataVK build_attachment_data(
    const RTRunStaticOpaqueTrianglesVK::Config& p_config, const BaseRenderDataVK* p_render_data) {
    StaticOpaqueAttachmentDataVK result;
    result.render_extent = p_config.get_render_extent();
    for (size_t i = 0; i < p_config.get_color_attachment_count(); ++i) {
        result.color_attachment_ids.emplace_back(p_config.get_color_attachment_id(i));
        result.color_attachment_formats.push_back(p_config.get_color_attachment_format(i));
    }
    for (size_t i = 0; i < p_config.get_clear_value_count(); ++i) {
        result.clear_values.push_back(p_config.get_clear_value(i));
    }
    if (result.color_attachment_ids.empty()) {
        result.color_attachment_ids.emplace_back(REND_RES_SWAP_CHAIN_IMAGE_VIEW_VK);
    }
    if (result.color_attachment_formats.empty()) {
        result.color_attachment_formats.push_back(p_render_data->swap_chain_format);
    }
    if (result.color_attachment_ids.size() != result.color_attachment_formats.size()) {
        throw std::runtime_error("Failed to construct static opaque task: color attachment ids and formats differ in count.");
    }
    if (p_config.has_depth_attachment() != (p_config.get_depth_attachment_format() != VK_FORMAT_UNDEFINED)) {
        throw std::runtime_error("Failed to construct static opaque task: depth attachment id and format must be provided together.");
    }
    if (result.render_extent.width == 0 || result.render_extent.height == 0) {
        result.render_extent = p_render_data->swap_chain_extent;
    }
    while (result.clear_values.size() < result.color_attachment_ids.size()) {
        result.clear_values.push_back(VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}});
    }
    if (p_config.has_depth_attachment() && result.clear_values.size() == result.color_attachment_ids.size()) {
        result.clear_values.push_back(VkClearValue{.depthStencil = {.depth = 1.0F, .stencil = 0}});
    }
    return result;
}

std::string make_pipeline_key(
    std::string_view p_resource_key, const std::vector<VkFormat>& p_color_formats, VkFormat p_depth_format, VkFormat p_stencil_format) {
    std::string result = std::format("{}#static_opaque_indirect", p_resource_key);
    for (VkFormat color_format : p_color_formats) {
        result += std::format("#color_{}", static_cast<int32_t>(color_format));
    }
    result += std::format("#depth_{}#stencil_{}", static_cast<int32_t>(p_depth_format), static_cast<int32_t>(p_stencil_format));
    return result;
}

StaticOpaqueCompiledPipelineBatchVK compile_pipeline_batch(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueDrawDataVK::PipelineBatch& p_draw_batch,
    const RTRunStaticOpaqueTrianglesVK::Config& p_config,
    GraphicsPipelineCompilerVKContext p_compiler_context,
    RefRes<RUMaterial> p_default_material,
    Ref<IRenderObject> p_material_sampler) {
    StaticOpaqueMaterialDataVK material_data = build_material_data(
        p_triangles_info, p_draw_batch, p_config, p_compiler_context.render_data, p_default_material, p_material_sampler);
    p_compiler_context.default_image_resource = material_data.default_image;
    StaticOpaqueCompiledPipelineBatchVK result;
    result.resource_key = make_pipeline_key(p_draw_batch.graphics_pipeline_key,
        p_compiler_context.color_attachment_formats,
        p_compiler_context.depth_attachment_format,
        p_compiler_context.stencil_attachment_format);
    result.compiled = GraphicsPipelineCompilerVK(p_compiler_context)
                          .compile(result.resource_key, p_draw_batch.graphics_pipeline, material_data.bindpoints);
    p_compiler_context.render_database->register_compiled_render_resource(
        result.resource_key, TypeUUID<ROGraphicsPipelineVK>::UUID, RefWeak<IRenderObject>(result.compiled.graphics_pipeline));
    result.first_draw = p_draw_batch.first_draw;
    result.draw_count = p_draw_batch.draw_count;
    return result;
}

Ref<ROBufferVK> build_indirect_commands_buffer(const StaticOpaqueDrawDataVK& p_draw_data, const BaseRenderDataVK* p_render_data) {
    size_t buffer_size = p_draw_data.indirect_commands.size() * sizeof(VkDrawIndexedIndirectCommand);
    Ref<ROBufferVK> result = ROBufferVK::Builder()
                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                 .set_size(buffer_size)
                                 .set_usage(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
                                 .create_ref(p_render_data);
    result->update_buffer(p_draw_data.indirect_commands.data(), buffer_size);
    return result;
}

Unique<RTRunGraphicsPipelineIndirectVK> build_indirect_task(RenderAllocator* p_allocator,
    const BaseRenderDataVK* p_render_data,
    StaticOpaqueAttachmentDataVK p_attachment_data,
    const RTRunStaticOpaqueTrianglesVK::Config& p_config,
    Ref<ROBufferVK> p_indirect_commands,
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch> p_pipeline_batches,
    VkAttachmentLoadOp p_color_load_op,
    VkAttachmentLoadOp p_depth_load_op) { // NOLINT
    std::vector<std::string_view> color_attachment_id_views;
    color_attachment_id_views.reserve(p_attachment_data.color_attachment_ids.size());
    for (const std::string& color_attachment_id : p_attachment_data.color_attachment_ids) {
        color_attachment_id_views.emplace_back(color_attachment_id);
    }
    std::optional<std::string> depth_attachment_id;
    if (p_config.has_depth_attachment()) {
        depth_attachment_id = p_config.get_depth_attachment_id();
    }
    std::optional<std::string_view> depth_attachment_id_view;
    if (depth_attachment_id.has_value()) {
        depth_attachment_id_view = *depth_attachment_id;
    }
    return make_unique<RTRunGraphicsPipelineIndirectVK>(p_allocator,
        p_render_data,
        std::span<const VkClearValue>(p_attachment_data.clear_values.data(), p_attachment_data.clear_values.size()),
        p_attachment_data.render_extent,
        std::span<const std::string_view>(color_attachment_id_views.data(), color_attachment_id_views.size()),
        depth_attachment_id_view,
        REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
        REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
        std::move(p_indirect_commands),
        std::span<const RTRunGraphicsPipelineIndirectVK::PipelineBatch>(p_pipeline_batches.data(), p_pipeline_batches.size()),
        p_color_load_op,
        p_depth_load_op);
}

std::string_view get_vertex_stage_entry_name(RefRes<RUGraphicsPipeline> p_pipeline) {
    if (p_pipeline == nullptr || p_pipeline->vertex_stage == nullptr) {
        throw std::runtime_error("Failed to construct static opaque task: vertex shader stage is missing.");
    }
    if (p_pipeline->vertex_stage_entry != nullptr && p_pipeline->vertex_stage_entry.get()[0] != '\0') {
        return p_pipeline->vertex_stage_entry.get();
    }
    if (p_pipeline->vertex_stage->vertex_entry != nullptr && p_pipeline->vertex_stage->vertex_entry.get()[0] != '\0') {
        return p_pipeline->vertex_stage->vertex_entry.get();
    }
    return {};
}

} // namespace

RTRunStaticOpaqueTrianglesVK::RTRunStaticOpaqueTrianglesVK(RendererVK* p_renderer, const Config& p_config) {
    validate_task_config(p_config);
    const BaseRenderDataVK* render_data = get_render_data(p_renderer);
    Ref<RenderDatabase> render_database_ref = p_renderer->get_render_database();
    RenderDatabase* render_database = render_database_ref.get();
    RenderAllocator* render_allocator = render_data->allocator;
    const auto& triangles_info = render_database->get_static_opaque_triangles_info();
    auto draw_data = build_draw_data(triangles_info, p_config, render_allocator);
    validate_indirect_draw_count(draw_data, render_data);
    auto attachment_data = build_attachment_data(p_config, render_data);
    RefRes<RUMaterial> default_material = get_default_material(p_config);
    auto material_sampler = ROSamplerVK::Builder()
                                .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                                .set_address_mode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                                .create_ref(render_data);
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = render_data,
        .render_database = render_database_ref,
        .renderer = p_renderer,
        .color_attachment_formats = attachment_data.color_attachment_formats,
        .depth_attachment_format = p_config.get_depth_attachment_format(),
        .stencil_attachment_format = p_config.get_stencil_attachment_format(),
    };
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch> task_pipeline_batches;
    for (const auto& draw_batch : draw_data.pipeline_batches) {
        StaticOpaqueCompiledPipelineBatchVK pipeline_batch =
            compile_pipeline_batch(triangles_info, draw_batch, p_config, compiler_context, default_material, material_sampler);
        ShaderMetadata vertex_metadata = ShaderMetadataLoader().load(draw_batch.graphics_pipeline->vertex_stage);
        const std::string_view vertex_entry_name = get_vertex_stage_entry_name(draw_batch.graphics_pipeline);
        std::vector<StaticOpaqueVertexBatchPlannerVK::Batch> vertex_batches = StaticOpaqueVertexBatchPlannerVK().build(
            vertex_metadata, vertex_entry_name, triangles_info, pipeline_batch.first_draw, pipeline_batch.draw_count);
        for (StaticOpaqueVertexBatchPlannerVK::Batch& vertex_batch : vertex_batches) {
            apply_vertex_offsets(draw_data, triangles_info, vertex_batch);
            RTRunGraphicsPipelineIndirectVK::PipelineBatch task_batch(render_allocator);
            task_batch.graphics_pipeline = pipeline_batch.compiled.graphics_pipeline;
            task_batch.first_draw = vertex_batch.first_draw;
            task_batch.draw_count = vertex_batch.draw_count;
            task_batch.vertex_buffer_offset = vertex_batch.vertex_buffer_offset;
            task_batch.vertex_input = std::move(vertex_batch.vertex_input);
            task_batch.descriptor_sets.assign(
                pipeline_batch.compiled.descriptor_sets.begin(), pipeline_batch.compiled.descriptor_sets.end());
            task_pipeline_batches.push_back(std::move(task_batch));
        }
        CompiledBatch compiled_batch;
        compiled_batch.descriptor_sets = std::move(pipeline_batch.compiled.descriptor_sets);
        compiled_batch.descriptor_set_bindings = std::move(pipeline_batch.compiled.descriptor_set_bindings);
        compiled_batches.push_back(std::move(compiled_batch));
    }
    Ref<ROBufferVK> indirect_commands = build_indirect_commands_buffer(draw_data, render_data);
    indirect_task = build_indirect_task(render_allocator,
        render_data,
        std::move(attachment_data),
        p_config,
        std::move(indirect_commands),
        std::move(task_pipeline_batches),
        p_config.get_color_attachment_load_op(),
        p_config.get_depth_attachment_load_op());
}

QueueFamilyVK RTRunStaticOpaqueTrianglesVK::queue_family() const {
    return indirect_task->queue_family();
}

Vector<ResAccessVK, RenderAllocator> RTRunStaticOpaqueTrianglesVK::read_resources() const {
    return indirect_task->read_resources();
}

Vector<ResAccessVK, RenderAllocator> RTRunStaticOpaqueTrianglesVK::write_resources() const {
    return indirect_task->write_resources();
}

void RTRunStaticOpaqueTrianglesVK::record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta) {
    indirect_task->record(p_command_buffer, p_resources, p_delta);
}

size_t RTRunStaticOpaqueTrianglesVK::get_compiled_batch_count() const {
    return compiled_batches.size();
}

Ref<RODescriptorSetVK> RTRunStaticOpaqueTrianglesVK::get_descriptor_set(size_t p_batch_index, size_t p_set_index) const {
    if (p_batch_index >= compiled_batches.size() || p_set_index >= compiled_batches[p_batch_index].descriptor_sets.size()) {
        throw std::runtime_error("Failed to get static opaque task descriptor set: batch or set index is out of bounds.");
    }
    return compiled_batches[p_batch_index].descriptor_sets[p_set_index];
}

RTRunStaticOpaqueTrianglesVK::DescriptorBinding RTRunStaticOpaqueTrianglesVK::get_descriptor_binding(
    size_t p_batch_index, std::string_view p_field_path) const {
    if (p_batch_index >= compiled_batches.size()) {
        throw std::runtime_error("Failed to get static opaque task descriptor binding: batch index is out of bounds.");
    }
    auto binding_it = compiled_batches[p_batch_index].descriptor_set_bindings.find(std::string(p_field_path));
    if (binding_it == compiled_batches[p_batch_index].descriptor_set_bindings.end()) {
        throw std::runtime_error(
            std::format("Failed to get static opaque task descriptor binding: field path '{}' was not found.", p_field_path));
    }
    auto [set_index, binding] = binding_it->second;
    return DescriptorBinding{.set_index = set_index, .binding = binding};
}

} // namespace WhiteBirdEngine
