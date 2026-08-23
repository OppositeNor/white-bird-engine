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
#include "function/render_vk/static_opaque_triangles_pipeline_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_weak.hh"
#include "core/memory/unique.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <array>
#include <cstddef>
#include <cstdint>
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

static constexpr uint32_t STATIC_OPAQUE_MATERIAL_TABLE_SIZE = 32;
static constexpr uint32_t STATIC_OPAQUE_TEXTURES_PER_MATERIAL = 3;
static constexpr uint32_t STATIC_OPAQUE_ALBEDO_TEXTURE_OFFSET = 0;
static constexpr uint32_t STATIC_OPAQUE_NORMAL_TEXTURE_OFFSET = 1;
static constexpr uint32_t STATIC_OPAQUE_ROUGHNESS_METALLIC_AO_TEXTURE_OFFSET = 2;

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_task_id(RendTaskID p_task_id) {
    task_id = p_task_id;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_default_material_resource_key(
    std::string_view p_resource_key) {
    default_material_resource_key = p_resource_key;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_material_textures_variable_name(
    std::string_view p_variable_name) {
    material_textures_variable_name = p_variable_name;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_material_params_variable_name(
    std::string_view p_variable_name) {
    material_params_variable_name = p_variable_name;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_graphics_pipeline_resource_key(
    std::string_view p_resource_key) {
    graphics_pipeline_resource_key = p_resource_key;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::add_graphics_pipeline_override(
    std::string_view p_source_resource_key, std::string_view p_target_resource_key) {
    PipelineOverride pipeline_override;
    pipeline_override.source_resource_key = p_source_resource_key;
    pipeline_override.target_resource_key = p_target_resource_key;
    graphics_pipeline_overrides.push_back(std::move(pipeline_override));
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_render_extent(VkExtent2D p_render_extent) {
    render_extent = p_render_extent;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::add_color_attachment(
    std::string_view p_resource_id, VkFormat p_format) {
    color_attachment_ids.emplace_back(p_resource_id);
    color_attachment_formats.push_back(p_format);
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_depth_attachment(
    std::string_view p_resource_id, VkFormat p_format) {
    depth_attachment_id = p_resource_id;
    depth_attachment_format = p_format;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_stencil_attachment_format(VkFormat p_format) {
    stencil_attachment_format = p_format;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::add_clear_value(VkClearValue p_clear_value) {
    clear_values.push_back(p_clear_value);
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_color_attachment_load_op(
    VkAttachmentLoadOp p_load_op) {
    color_attachment_load_op = p_load_op;
    return *this;
}

StaticOpaqueTrianglesPipelineVK::Config& StaticOpaqueTrianglesPipelineVK::Config::set_depth_attachment_load_op(
    VkAttachmentLoadOp p_load_op) {
    depth_attachment_load_op = p_load_op;
    return *this;
}

RendTaskID StaticOpaqueTrianglesPipelineVK::Config::get_task_id() const {
    return task_id;
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_default_material_resource_key() const {
    return default_material_resource_key;
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_material_textures_variable_name() const {
    return material_textures_variable_name;
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_material_params_variable_name() const {
    return material_params_variable_name;
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_graphics_pipeline_resource_key() const {
    return graphics_pipeline_resource_key;
}

size_t StaticOpaqueTrianglesPipelineVK::Config::get_graphics_pipeline_override_count() const {
    return graphics_pipeline_overrides.size();
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_graphics_pipeline_override_source(size_t p_index) const {
    if (p_index >= graphics_pipeline_overrides.size()) {
        throw std::runtime_error("Failed to read static opaque pipeline configuration: pipeline override index is out of bounds.");
    }
    return graphics_pipeline_overrides[p_index].source_resource_key;
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_graphics_pipeline_override_target(size_t p_index) const {
    if (p_index >= graphics_pipeline_overrides.size()) {
        throw std::runtime_error("Failed to read static opaque pipeline configuration: pipeline override index is out of bounds.");
    }
    return graphics_pipeline_overrides[p_index].target_resource_key;
}

VkExtent2D StaticOpaqueTrianglesPipelineVK::Config::get_render_extent() const {
    return render_extent;
}

size_t StaticOpaqueTrianglesPipelineVK::Config::get_color_attachment_count() const {
    return color_attachment_ids.size();
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_color_attachment_id(size_t p_index) const {
    if (p_index >= color_attachment_ids.size()) {
        throw std::runtime_error("Failed to read static opaque pipeline configuration: color attachment index is out of bounds.");
    }
    return color_attachment_ids[p_index];
}

VkFormat StaticOpaqueTrianglesPipelineVK::Config::get_color_attachment_format(size_t p_index) const {
    if (p_index >= color_attachment_formats.size()) {
        throw std::runtime_error("Failed to read static opaque pipeline configuration: color attachment index is out of bounds.");
    }
    return color_attachment_formats[p_index];
}

bool StaticOpaqueTrianglesPipelineVK::Config::has_depth_attachment() const {
    return depth_attachment_id.has_value();
}

std::string_view StaticOpaqueTrianglesPipelineVK::Config::get_depth_attachment_id() const {
    if (!depth_attachment_id.has_value()) {
        throw std::runtime_error("Failed to read static opaque pipeline configuration: no depth attachment is configured.");
    }
    return *depth_attachment_id;
}

VkFormat StaticOpaqueTrianglesPipelineVK::Config::get_depth_attachment_format() const {
    return depth_attachment_format;
}

VkFormat StaticOpaqueTrianglesPipelineVK::Config::get_stencil_attachment_format() const {
    return stencil_attachment_format;
}

size_t StaticOpaqueTrianglesPipelineVK::Config::get_clear_value_count() const {
    return clear_values.size();
}

VkClearValue StaticOpaqueTrianglesPipelineVK::Config::get_clear_value(size_t p_index) const {
    if (p_index >= clear_values.size()) {
        throw std::runtime_error("Failed to read static opaque pipeline configuration: clear value index is out of bounds.");
    }
    return clear_values[p_index];
}

VkAttachmentLoadOp StaticOpaqueTrianglesPipelineVK::Config::get_color_attachment_load_op() const {
    return color_attachment_load_op;
}

VkAttachmentLoadOp StaticOpaqueTrianglesPipelineVK::Config::get_depth_attachment_load_op() const {
    return depth_attachment_load_op;
}

struct alignas(16) StaticOpaqueMaterialParamsVK {
    int32_t albedo_index = STATIC_OPAQUE_ALBEDO_TEXTURE_OFFSET;
    int32_t normal_index = STATIC_OPAQUE_NORMAL_TEXTURE_OFFSET;
    int32_t roughness_metallic_ao_index = STATIC_OPAQUE_ROUGHNESS_METALLIC_AO_TEXTURE_OFFSET;
    int32_t padding = 0;
};

struct StaticOpaqueMaterialParamsTableVK {
    std::array<StaticOpaqueMaterialParamsVK, STATIC_OPAQUE_MATERIAL_TABLE_SIZE> materials{};
};

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

static RenderAllocator* get_render_allocator(RendererVK* p_renderer) {
    if (p_renderer == nullptr || p_renderer->get_base_render_data() == nullptr || p_renderer->get_base_render_data()->allocator == nullptr) {
        throw std::runtime_error("Failed to construct static opaque pipeline: renderer state is missing.");
    }
    return p_renderer->get_base_render_data()->allocator;
}

struct CookTorranceMaterialTexturesVK {
    CookTorranceMaterialTexturesVK() = default;
    ~CookTorranceMaterialTexturesVK() = default;
    CookTorranceMaterialTexturesVK(const CookTorranceMaterialTexturesVK&) = default;
    CookTorranceMaterialTexturesVK(CookTorranceMaterialTexturesVK&&) noexcept = default;
    CookTorranceMaterialTexturesVK& operator=(const CookTorranceMaterialTexturesVK&) = default;
    CookTorranceMaterialTexturesVK& operator=(CookTorranceMaterialTexturesVK&&) noexcept = default;

    RefRes<RUImage> albedo = MEM_NULL;
    RefRes<RUImage> normal = MEM_NULL;
    RefRes<RUImage> rma = MEM_NULL;
};

static size_t get_cook_torrance_texture_type_index(HashCode p_texture_type) {
    if (dynam_hash("albedo") == p_texture_type) {
        return 0;
    }
    if (dynam_hash("normal") == p_texture_type) {
        return 1;
    }
    if (dynam_hash("rma") == p_texture_type) {
        return 2;
    }
    throw std::runtime_error("Failed to construct static opaque pipeline: material contains an invalid texture type.");
}

static CookTorranceMaterialTexturesVK unpack_cook_torrance_textures(RefRes<RUMaterial> p_material) {
    if (p_material == nullptr) {
        throw std::runtime_error("Failed to construct static opaque pipeline: material is missing.");
    }
    if (dynam_hash("cook_torrance") != p_material->material_type) {
        throw std::runtime_error("Failed to construct static opaque pipeline: material type is not supported.");
    }
    CookTorranceMaterialTexturesVK result;
    std::array<bool, 3> texture_type_present{};
    std::span<const RUMaterial::TextureSlot> texture_slots(p_material->textures.get(), p_material->texture_count);
    // TODO: Make material unpacking generic by grouping slots by material type and assigning texture indices automatically.
    for (const RUMaterial::TextureSlot& texture_slot : texture_slots) {
        size_t texture_type_index = get_cook_torrance_texture_type_index(texture_slot.texture_type);
        if (texture_type_present[texture_type_index]) {
            throw std::runtime_error("Failed to construct static opaque pipeline: material contains a duplicate texture type.");
        }
        texture_type_present[texture_type_index] = true;
        if (dynam_hash("albedo") == texture_slot.texture_type) {
            result.albedo = texture_slot.texture;
        } else if (dynam_hash("normal") == texture_slot.texture_type) {
            result.normal = texture_slot.texture;
        } else if (dynam_hash("rma") == texture_slot.texture_type) {
            result.rma = texture_slot.texture;
        }
    }
    return result;
}

static void add_material_texture_binding(RenderBindpointDeclarationsBuilder& p_builder,
    std::string_view p_material_textures_variable_name,
    RefRes<RUImage> p_texture,
    uint32_t p_material_index,
    uint32_t p_texture_offset,
    int32_t& p_texture_index) {
    if (p_texture == nullptr) {
        return;
    }
    uint32_t texture_index = p_material_index * STATIC_OPAQUE_TEXTURES_PER_MATERIAL + p_texture_offset;
    p_builder.add_image_decleration(
        std::format("{}.textures[{}]", p_material_textures_variable_name, texture_index), p_texture);
    p_texture_index = static_cast<int32_t>(texture_index);
}

static void add_material_bindings(RenderBindpointDeclarationsBuilder& p_builder,
    std::string_view p_material_textures_variable_name,
    RefRes<RUMaterial> p_material,
    uint32_t p_material_index,
    StaticOpaqueMaterialParamsVK& p_material_params) {
    CookTorranceMaterialTexturesVK textures = unpack_cook_torrance_textures(p_material);
    add_material_texture_binding(p_builder,
        p_material_textures_variable_name,
        textures.albedo,
        p_material_index,
        STATIC_OPAQUE_ALBEDO_TEXTURE_OFFSET,
        p_material_params.albedo_index);
    add_material_texture_binding(p_builder,
        p_material_textures_variable_name,
        textures.normal,
        p_material_index,
        STATIC_OPAQUE_NORMAL_TEXTURE_OFFSET,
        p_material_params.normal_index);
    add_material_texture_binding(p_builder,
        p_material_textures_variable_name,
        textures.rma,
        p_material_index,
        STATIC_OPAQUE_ROUGHNESS_METALLIC_AO_TEXTURE_OFFSET,
        p_material_params.roughness_metallic_ao_index);
}

static void validate_draw_data_source(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info) {
    if (p_triangles_info.vertices.empty() || p_triangles_info.indices.empty() || p_triangles_info.materials.empty() ||
        p_triangles_info.graphics_pipelines.empty()) {
        throw std::runtime_error("Failed to construct static opaque pipeline: the render database has no drawable triangles.");
    }
    if (p_triangles_info.materials.size() >= STATIC_OPAQUE_MATERIAL_TABLE_SIZE) {
        throw std::runtime_error(std::format("Failed to construct static opaque pipeline: {} material batches exceed the maximum of {}.",
            p_triangles_info.materials.size(),
            STATIC_OPAQUE_MATERIAL_TABLE_SIZE - 1));
    }
}

static void append_indirect_commands(StaticOpaqueDrawDataVK& p_draw_data, const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info) {
    p_draw_data.indirect_commands.reserve(p_triangles_info.materials.size());
    for (uint32_t material_index = 0; material_index < p_triangles_info.materials.size(); ++material_index) {
        const auto& material_unit = p_triangles_info.materials[material_index];
        if (material_unit.sub_region.starting_index > p_triangles_info.indices.size() ||
            material_unit.sub_region.size > p_triangles_info.indices.size() - material_unit.sub_region.starting_index ||
            material_unit.sub_region.starting_index > std::numeric_limits<uint32_t>::max() ||
            material_unit.sub_region.size > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to construct static opaque pipeline: a material index range is invalid.");
        }
        p_draw_data.indirect_commands.push_back(VkDrawIndexedIndirectCommand{
            .indexCount = static_cast<uint32_t>(material_unit.sub_region.size),
            .instanceCount = 1,
            .firstIndex = static_cast<uint32_t>(material_unit.sub_region.starting_index),
            .vertexOffset = 0,
            .firstInstance = material_index + 1,
        });
    }
}

static void validate_pipeline_overrides(const StaticOpaqueTrianglesPipelineVK::Config& p_config) {
    for (size_t i = 0; i < p_config.get_graphics_pipeline_override_count(); ++i) {
        if (p_config.get_graphics_pipeline_override_source(i).empty() || p_config.get_graphics_pipeline_override_target(i).empty()) {
            throw std::runtime_error("Failed to construct static opaque pipeline: pipeline override keys must not be empty.");
        }
    }
}

static std::string resolve_target_pipeline_key(
    std::string_view p_source_pipeline_key, const StaticOpaqueTrianglesPipelineVK::Config& p_config) {
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

static void append_pipeline_batch(StaticOpaqueDrawDataVK& p_draw_data,
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

static void append_pipeline_batches(StaticOpaqueDrawDataVK& p_draw_data,
    const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueTrianglesPipelineVK::Config& p_config) {
    auto* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr) {
        throw std::runtime_error("Failed to construct static opaque pipeline: resource manager is missing.");
    }
    validate_pipeline_overrides(p_config);
    for (const auto& pipeline_unit : p_triangles_info.graphics_pipelines) {
        if (pipeline_unit.graphics_pipeline == nullptr || pipeline_unit.graphics_pipeline->resource_key == nullptr) {
            throw std::runtime_error("Failed to construct static opaque pipeline: a material graphics pipeline is missing.");
        }
        if (pipeline_unit.material_sub_region.starting_index > p_triangles_info.materials.size() ||
            pipeline_unit.material_sub_region.size > p_triangles_info.materials.size() - pipeline_unit.material_sub_region.starting_index ||
            pipeline_unit.material_sub_region.starting_index > std::numeric_limits<uint32_t>::max() ||
            pipeline_unit.material_sub_region.size > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to construct static opaque pipeline: a graphics-pipeline material range is invalid.");
        }
        std::string source_pipeline_key = pipeline_unit.graphics_pipeline->resource_key;
        std::string target_pipeline_key = resolve_target_pipeline_key(source_pipeline_key, p_config);
        RefRes<RUGraphicsPipeline> target_pipeline = target_pipeline_key == source_pipeline_key
                                                         ? pipeline_unit.graphics_pipeline
                                                         : resource_manager->get_resource<RUGraphicsPipeline>(target_pipeline_key);
        if (target_pipeline == nullptr) {
            throw std::runtime_error(std::format(
                "Failed to construct static opaque pipeline: graphics pipeline '{}' was not found.", target_pipeline_key));
        }
        uint32_t first_draw = static_cast<uint32_t>(pipeline_unit.material_sub_region.starting_index);
        uint32_t draw_count = static_cast<uint32_t>(pipeline_unit.material_sub_region.size);
        append_pipeline_batch(p_draw_data, target_pipeline, std::move(target_pipeline_key), first_draw, draw_count);
    }
}

static StaticOpaqueDrawDataVK build_draw_data(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueTrianglesPipelineVK::Config& p_config,
    RenderAllocator* p_allocator) {
    validate_draw_data_source(p_triangles_info);
    StaticOpaqueDrawDataVK result(p_allocator);
    append_indirect_commands(result, p_triangles_info);
    append_pipeline_batches(result, p_triangles_info, p_config);
    return result;
}

static StaticOpaqueMaterialDataVK build_material_data(const RenderDatabase::OpaqueTrianglesInfo& p_triangles_info,
    const StaticOpaqueTrianglesPipelineVK::Config& p_config,
    const BaseRenderDataVK* p_render_data) {
    auto* resource_manager = ResourceManager::get_singleton();
    RefRes<RUMaterial> default_material = resource_manager->get_resource<RUMaterial>(p_config.get_default_material_resource_key());
    CookTorranceMaterialTexturesVK default_textures = unpack_cook_torrance_textures(default_material);
    if (default_textures.albedo == nullptr || default_textures.normal == nullptr || default_textures.rma == nullptr) {
        throw std::runtime_error(std::format("Failed to construct static opaque pipeline: '{}' must provide albedo, normal, and "
                                             "roughness-metallic-ambient-occlusion textures.",
            p_config.get_default_material_resource_key()));
    }
    auto material_sampler = ROSamplerVK::Builder()
                                .set_filter(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
                                .set_address_mode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                                .create_ref(p_render_data);
    RenderBindpointDeclarationsBuilder bindpoint_builder;
    StaticOpaqueMaterialParamsTableVK material_params;
    bindpoint_builder.add_sampler_decleration(
        std::format("{}.material_sampler", p_config.get_material_textures_variable_name()), material_sampler);
    add_material_bindings(
        bindpoint_builder, p_config.get_material_textures_variable_name(), default_material, 0, material_params.materials[0]);
    for (uint32_t material_index = 0; material_index < p_triangles_info.materials.size(); ++material_index) {
        uint32_t table_index = material_index + 1;
        add_material_bindings(bindpoint_builder,
            p_config.get_material_textures_variable_name(),
            p_triangles_info.materials[material_index].material,
            table_index,
            material_params.materials[table_index]);
    }
    bindpoint_builder.add_buffer_decleration(p_config.get_material_params_variable_name(), &material_params, sizeof(material_params));
    StaticOpaqueMaterialDataVK result;
    result.bindpoints = bindpoint_builder.build();
    result.default_image = default_textures.albedo;
    return result;
}

static StaticOpaqueAttachmentDataVK build_attachment_data(
    const StaticOpaqueTrianglesPipelineVK::Config& p_config, const BaseRenderDataVK* p_render_data) {
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
        throw std::runtime_error("Failed to construct static opaque pipeline: color attachment ids and formats differ in count.");
    }
    if (p_config.has_depth_attachment() != (p_config.get_depth_attachment_format() != VK_FORMAT_UNDEFINED)) {
        throw std::runtime_error("Failed to construct static opaque pipeline: depth attachment id and format must be provided together.");
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

static std::string make_pipeline_key(
    std::string_view p_resource_key, const std::vector<VkFormat>& p_color_formats, VkFormat p_depth_format, VkFormat p_stencil_format) {
    std::string result = std::format("{}#static_opaque_indirect", p_resource_key);
    for (VkFormat color_format : p_color_formats) {
        result += std::format("#color_{}", static_cast<int32_t>(color_format));
    }
    result += std::format("#depth_{}#stencil_{}", static_cast<int32_t>(p_depth_format), static_cast<int32_t>(p_stencil_format));
    return result;
}

static std::function<Unique<RenderTaskVK>()> make_task_factory(RenderAllocator* p_allocator,
    const BaseRenderDataVK* p_render_data,
    StaticOpaqueAttachmentDataVK p_attachment_data,
    std::optional<std::string> p_depth_attachment_id,
    std::string p_indirect_commands_resource_key,
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch> p_pipeline_batches,
    VkAttachmentLoadOp p_color_load_op,
    VkAttachmentLoadOp p_depth_load_op) { // NOLINT
    return [task_allocator = p_allocator,
               task_render_data = p_render_data,
               task_attachment_data = std::move(p_attachment_data),
               task_depth_attachment_id = std::move(p_depth_attachment_id),
               task_indirect_commands_resource_key = std::move(p_indirect_commands_resource_key),
               task_pipeline_batches = std::move(p_pipeline_batches),
               task_color_load_op = p_color_load_op,
               task_depth_load_op = p_depth_load_op]() -> Unique<RenderTaskVK> {
        std::vector<std::string_view> color_attachment_id_views;
        color_attachment_id_views.reserve(task_attachment_data.color_attachment_ids.size());
        for (const std::string& color_attachment_id : task_attachment_data.color_attachment_ids) {
            color_attachment_id_views.emplace_back(color_attachment_id);
        }
        std::optional<std::string_view> depth_attachment_id_view;
        if (task_depth_attachment_id.has_value()) {
            depth_attachment_id_view = *task_depth_attachment_id;
        }
        return make_unique<RTRunGraphicsPipelineIndirectVK>(task_allocator,
            task_render_data,
            std::span<const VkClearValue>(task_attachment_data.clear_values.data(), task_attachment_data.clear_values.size()),
            task_attachment_data.render_extent,
            std::span<const std::string_view>(color_attachment_id_views.data(), color_attachment_id_views.size()),
            depth_attachment_id_view,
            REND_RES_STATIC_OPAQUE_TRIANGLES_VERTICES_VK,
            REND_RES_STATIC_OPAQUE_TRIANGLES_INDICES_VK,
            task_indirect_commands_resource_key,
            std::span<const RTRunGraphicsPipelineIndirectVK::PipelineBatch>(
                task_pipeline_batches.data(), task_pipeline_batches.size()),
            task_color_load_op,
            task_depth_load_op);
    };
}

StaticOpaqueTrianglesPipelineVK::StaticOpaqueTrianglesPipelineVK(RendererVK* p_renderer, const Config& p_config)
    : task(get_render_allocator(p_renderer)) {
    if (p_config.get_default_material_resource_key().empty() || p_config.get_material_textures_variable_name().empty() ||
        p_config.get_material_params_variable_name().empty()) {
        throw std::runtime_error("Failed to construct static opaque pipeline: material contract base names must not be empty.");
    }
    const BaseRenderDataVK* render_data = p_renderer->get_base_render_data();
    Ref<RenderDatabase> render_database_ref = p_renderer->get_render_database();
    RenderDatabase* render_database = render_database_ref.get();
    RenderAllocator* render_allocator = render_data->allocator;
    const auto& triangles_info = render_database->get_static_opaque_triangles_info();
    auto draw_data = build_draw_data(triangles_info, p_config, render_allocator);
    if (draw_data.indirect_commands.size() > render_data->max_draw_indirect_count) {
        throw std::runtime_error(std::format("Failed to construct static opaque pipeline: {} draws exceed the device limit of {}.",
            draw_data.indirect_commands.size(),
            render_data->max_draw_indirect_count));
    }
    auto material_data = build_material_data(triangles_info, p_config, render_data);
    auto attachment_data = build_attachment_data(p_config, render_data);

    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_allocator,
        .render_data = render_data,
        .render_database = render_database_ref,
        .renderer = p_renderer,
        .default_image_resource = material_data.default_image,
        .color_attachment_formats = attachment_data.color_attachment_formats,
        .depth_attachment_format = p_config.get_depth_attachment_format(),
        .stencil_attachment_format = p_config.get_stencil_attachment_format(),
    };
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch> task_pipeline_batches;
    for (const auto& draw_batch : draw_data.pipeline_batches) {
        std::string compiled_pipeline_key = make_pipeline_key(draw_batch.graphics_pipeline_key,
            attachment_data.color_attachment_formats,
            p_config.get_depth_attachment_format(),
            p_config.get_stencil_attachment_format());
        auto compiled = GraphicsPipelineCompilerVK(compiler_context)
                            .compile(compiled_pipeline_key, draw_batch.graphics_pipeline, material_data.bindpoints);
        render_database->register_compiled_render_resource(
            compiled_pipeline_key, TypeUUID<ROGraphicsPipelineVK>::UUID, RefWeak<IRenderObject>(compiled.graphics_pipeline));
        GraphicsPipelineResource pipeline_resource;
        pipeline_resource.resource_key = compiled_pipeline_key;
        pipeline_resource.graphics_pipeline = compiled.graphics_pipeline;
        graphics_pipeline_resources.push_back(std::move(pipeline_resource));
        RTRunGraphicsPipelineIndirectVK::PipelineBatch task_batch(render_allocator);
        task_batch.pipeline_id = compiled_pipeline_key;
        task_batch.first_draw = draw_batch.first_draw;
        task_batch.draw_count = draw_batch.draw_count;
        task_batch.descriptor_sets.assign(compiled.descriptor_sets.begin(), compiled.descriptor_sets.end());
        task_pipeline_batches.push_back(std::move(task_batch));
        CompiledBatch compiled_batch;
        compiled_batch.descriptor_sets = std::move(compiled.descriptor_sets);
        compiled_batch.descriptor_set_bindings = std::move(compiled.descriptor_set_bindings);
        compiled_batches.push_back(std::move(compiled_batch));
    }

    size_t indirect_buffer_size = draw_data.indirect_commands.size() * sizeof(VkDrawIndexedIndirectCommand);
    indirect_commands = ROBufferVK::Builder()
                            .set_domain(DeviceDomain::HOST_DEVICE)
                            .set_size(indirect_buffer_size)
                            .set_usage(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
                            .create_ref(render_data);
    indirect_commands->update_buffer(draw_data.indirect_commands.data(), indirect_buffer_size);
    indirect_commands_resource_key =
        std::format("{}#{}", REND_RES_STATIC_OPAQUE_TRIANGLES_INDIRECT_COMMANDS_VK, p_config.get_task_id());

    task.task_id = p_config.get_task_id();
    std::optional<std::string> depth_attachment_id;
    if (p_config.has_depth_attachment()) {
        depth_attachment_id = p_config.get_depth_attachment_id();
    }
    task.create = make_task_factory(render_allocator,
        render_data,
        std::move(attachment_data),
        std::move(depth_attachment_id),
        indirect_commands_resource_key,
        std::move(task_pipeline_batches),
        p_config.get_color_attachment_load_op(),
        p_config.get_depth_attachment_load_op());
}

void StaticOpaqueTrianglesPipelineVK::add_resources(RendGraphDescVK& p_graph_desc) const {
    p_graph_desc.resources.emplace_back(
        RendResVK{.res_id = indirect_commands_resource_key, .res_type_id = TypeUUID<ROBufferVK>::UUID, .res = indirect_commands});
    for (const auto& pipeline_resource : graphics_pipeline_resources) {
        p_graph_desc.resources.emplace_back(RendResVK{.res_id = pipeline_resource.resource_key,
            .res_type_id = TypeUUID<ROGraphicsPipelineVK>::UUID,
            .res = pipeline_resource.graphics_pipeline});
    }
}

size_t StaticOpaqueTrianglesPipelineVK::get_compiled_batch_count() const {
    return compiled_batches.size();
}

Ref<RODescriptorSetVK> StaticOpaqueTrianglesPipelineVK::get_descriptor_set(size_t p_batch_index, size_t p_set_index) const {
    if (p_batch_index >= compiled_batches.size() || p_set_index >= compiled_batches[p_batch_index].descriptor_sets.size()) {
        throw std::runtime_error("Failed to get static opaque pipeline descriptor set: batch or set index is out of bounds.");
    }
    return compiled_batches[p_batch_index].descriptor_sets[p_set_index];
}

StaticOpaqueTrianglesPipelineVK::DescriptorBinding StaticOpaqueTrianglesPipelineVK::get_descriptor_binding(
    size_t p_batch_index, std::string_view p_field_path) const {
    if (p_batch_index >= compiled_batches.size()) {
        throw std::runtime_error("Failed to get static opaque pipeline descriptor binding: batch index is out of bounds.");
    }
    auto binding_it = compiled_batches[p_batch_index].descriptor_set_bindings.find(std::string(p_field_path));
    if (binding_it == compiled_batches[p_batch_index].descriptor_set_bindings.end()) {
        throw std::runtime_error(
            std::format("Failed to get static opaque pipeline descriptor binding: field path '{}' was not found.", p_field_path));
    }
    auto [set_index, binding] = binding_it->second;
    return DescriptorBinding{.set_index = set_index, .binding = binding};
}

} // namespace WhiteBirdEngine
