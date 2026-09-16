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
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/material_texture_contract_resolver.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render/static_set_node_contract.hh"
#include "function/render/static_set_node_contract_resolver.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/static_set_buffer_compiler_vk.hh"
#include "function/render_vk/static_set_draw_batch_planner_vk.hh"
#include "function/render_vk/static_set_material_binding_builder_vk.hh"
#include "glm/common.hpp"
#include "glm/matrix.hpp"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_inverse.hpp>
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

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_default_material_resource_key(std::string_view p_resource_key) {
    default_material_resource_key = p_resource_key;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_material_textures_variable_name(std::string_view p_variable_name) {
    material_textures_variable_name = p_variable_name;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_material_params_variable_name(std::string_view p_variable_name) {
    material_params_variable_name = p_variable_name;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_render_extent(VkExtent2D p_render_extent) {
    render_extent = p_render_extent;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::add_color_attachment(std::string_view p_resource_id, VkFormat p_format) {
    color_attachment_ids.emplace_back(p_resource_id);
    color_attachment_formats.push_back(p_format);
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_depth_attachment(std::string_view p_resource_id, VkFormat p_format) {
    depth_attachment_id = p_resource_id;
    depth_attachment_format = p_format;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_stencil_attachment_format(VkFormat p_format) {
    stencil_attachment_format = p_format;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::add_clear_value(VkClearValue p_clear_value) {
    clear_values.push_back(p_clear_value);
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_color_attachment_load_op(VkAttachmentLoadOp p_load_op) {
    color_attachment_load_op = p_load_op;
    return *this;
}

RTRunStaticSetVK::Config& RTRunStaticSetVK::Config::set_depth_attachment_load_op(VkAttachmentLoadOp p_load_op) {
    depth_attachment_load_op = p_load_op;
    return *this;
}

std::string_view RTRunStaticSetVK::Config::get_default_material_resource_key() const {
    return default_material_resource_key;
}

std::string_view RTRunStaticSetVK::Config::get_material_textures_variable_name() const {
    return material_textures_variable_name;
}

std::string_view RTRunStaticSetVK::Config::get_material_params_variable_name() const {
    return material_params_variable_name;
}

VkExtent2D RTRunStaticSetVK::Config::get_render_extent() const {
    return render_extent;
}

size_t RTRunStaticSetVK::Config::get_color_attachment_count() const {
    return color_attachment_ids.size();
}

std::string_view RTRunStaticSetVK::Config::get_color_attachment_id(size_t p_index) const {
    if (p_index >= color_attachment_ids.size()) {
        throw std::runtime_error("Failed to read static set task configuration: color attachment index is out of bounds.");
    }
    return color_attachment_ids[p_index];
}

VkFormat RTRunStaticSetVK::Config::get_color_attachment_format(size_t p_index) const {
    if (p_index >= color_attachment_formats.size()) {
        throw std::runtime_error("Failed to read static set task configuration: color attachment index is out of bounds.");
    }
    return color_attachment_formats[p_index];
}

bool RTRunStaticSetVK::Config::has_depth_attachment() const {
    return depth_attachment_id.has_value();
}

std::string_view RTRunStaticSetVK::Config::get_depth_attachment_id() const {
    if (!depth_attachment_id.has_value()) {
        throw std::runtime_error("Failed to read static set task configuration: no depth attachment is configured.");
    }
    return *depth_attachment_id;
}

VkFormat RTRunStaticSetVK::Config::get_depth_attachment_format() const {
    return depth_attachment_format;
}

VkFormat RTRunStaticSetVK::Config::get_stencil_attachment_format() const {
    return stencil_attachment_format;
}

size_t RTRunStaticSetVK::Config::get_clear_value_count() const {
    return clear_values.size();
}

VkClearValue RTRunStaticSetVK::Config::get_clear_value(size_t p_index) const {
    if (p_index >= clear_values.size()) {
        throw std::runtime_error("Failed to read static set task configuration: clear value index is out of bounds.");
    }
    return clear_values[p_index];
}

VkAttachmentLoadOp RTRunStaticSetVK::Config::get_color_attachment_load_op() const {
    return color_attachment_load_op;
}

VkAttachmentLoadOp RTRunStaticSetVK::Config::get_depth_attachment_load_op() const {
    return depth_attachment_load_op;
}

namespace {

struct StaticSetAttachmentDataVK {
    VkExtent2D render_extent{};
    std::vector<std::string> color_attachment_ids;
    std::optional<std::string> depth_attachment_id;
    std::vector<VkClearValue> clear_values;
};

struct StaticSetSubmeshGPU {
    uint32_t material_id = 0;
    uint32_t padding_0 = 0;
    uint32_t padding_1 = 0;
    uint32_t padding_2 = 0;
};

struct StaticSetNodeGPU {
    glm::mat4 transform{1.0F};
    glm::mat4 normal_transform{1.0F};
};

struct StaticSetInstanceGPU {
    uint32_t submesh_index = 0;
    uint32_t node_index = 0;
    uint32_t padding_0 = 0;
    uint32_t padding_1 = 0;
};

struct StaticInstanceBufferData {
    std::vector<StaticSetSubmeshGPU> submeshes;
    std::vector<StaticSetNodeGPU> nodes;
    std::vector<StaticSetInstanceGPU> instances;
    std::vector<uint32_t> first_instances;
    std::vector<RefRes<RUMaterial>> ordered_materials;
};

struct NonEmptyStaticSetTaskBuild {
    Unique<RTRunGraphicsPipelineIndirectVK> indirect_task = MEM_NULL;
    Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
};

struct StaticSetStorageBuffers {
    StaticInstanceBufferData instance_data;
    Ref<ROBufferVK> submesh_buffer = MEM_NULL;
    Ref<ROBufferVK> node_buffer = MEM_NULL;
    Ref<ROBufferVK> instance_buffer = MEM_NULL;
};

struct IndirectDrawPlanData {
    explicit IndirectDrawPlanData(RenderAllocator* p_allocator) : draw_plan(p_allocator), pipeline_batches(p_allocator) {
    }

    StaticSetDrawBatchPlannerVK::BuildResult draw_plan;
    Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator> pipeline_batches;
};

StaticInstanceBufferData build_static_instance_buffer_data(RefRes<RUStaticGeometrySet> p_set);
std::vector<std::string_view> color_attachment_id_views(const StaticSetAttachmentDataVK& p_attachment_data);
std::optional<std::string_view> depth_attachment_id_view(const StaticSetAttachmentDataVK& p_attachment_data);

StaticSetAttachmentDataVK build_attachment_data(const RTRunStaticSetVK::Config& p_config, const BaseRenderDataVK* p_render_data) {
    StaticSetAttachmentDataVK result;
    result.render_extent = p_config.get_render_extent();
    for (size_t i = 0; i < p_config.get_color_attachment_count(); ++i) {
        result.color_attachment_ids.emplace_back(p_config.get_color_attachment_id(i));
        static_cast<void>(p_config.get_color_attachment_format(i));
    }
    for (size_t i = 0; i < p_config.get_clear_value_count(); ++i) {
        result.clear_values.push_back(p_config.get_clear_value(i));
    }
    if (result.color_attachment_ids.empty()) {
        result.color_attachment_ids.emplace_back(REND_RES_SWAP_CHAIN_IMAGE);
    }
    if (p_config.has_depth_attachment() != (p_config.get_depth_attachment_format() != VK_FORMAT_UNDEFINED)) {
        throw std::runtime_error("Failed to construct static set task: depth attachment id and format must be provided together.");
    }
    if (p_config.has_depth_attachment()) {
        result.depth_attachment_id = std::string(p_config.get_depth_attachment_id());
    }
    if (result.render_extent.width == 0 || result.render_extent.height == 0) {
        result.render_extent = p_render_data->swap_chain_extent;
    }
    while (result.clear_values.size() < result.color_attachment_ids.size()) {
        result.clear_values.push_back(VkClearValue{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}});
    }
    if (result.depth_attachment_id.has_value() && result.clear_values.size() == result.color_attachment_ids.size()) {
        result.clear_values.push_back(VkClearValue{.depthStencil = {.depth = 0.0F, .stencil = 0}});
    }
    return result;
}

void validate_constructor_inputs(RendererVK* p_renderer, RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_graphics_pipeline) {
    if (p_renderer == nullptr || p_renderer->get_base_render_data() == nullptr || p_renderer->get_base_render_data()->allocator == nullptr) {
        throw std::runtime_error("Failed to construct static set task: renderer state is missing.");
    }
    if (p_set == nullptr) {
        throw std::runtime_error("Failed to construct static set task: static geometry set is missing.");
    }
    if (p_graphics_pipeline == nullptr) {
        throw std::runtime_error("Failed to construct static set task: graphics pipeline is missing.");
    }
}

bool is_empty_set(const RUStaticGeometrySet& p_set) {
    return p_set.submesh_count == 0 || p_set.instance_count == 0;
}

Ref<ROBufferVK> build_indirect_buffer(const BaseRenderDataVK* p_render_data, std::span<const VkDrawIndexedIndirectCommand> p_commands) {
    if (p_commands.empty()) {
        return MEM_NULL;
    }
    const size_t buffer_size = p_commands.size() * sizeof(VkDrawIndexedIndirectCommand);
    Ref<ROBufferVK> result = ROBufferVK::Builder()
                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                 .set_size(buffer_size)
                                 .set_usage(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
                                 .create_ref(p_render_data);
    result->update_buffer(p_commands.data(), buffer_size);
    return result;
}

template <typename TValue>
Ref<ROBufferVK> build_storage_buffer(
    const BaseRenderDataVK* p_render_data, const std::vector<TValue>& p_values, std::optional<uint32_t> p_descriptor_index) {
    if (p_values.empty()) {
        return MEM_NULL;
    }
    const size_t buffer_size = p_values.size() * sizeof(TValue);
    ROBufferVK::Builder builder;
    builder.set_domain(DeviceDomain::HOST_DEVICE).set_size(buffer_size).set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (p_descriptor_index.has_value()) {
        builder.set_storage_view_index(*p_descriptor_index);
    }
    Ref<ROBufferVK> result = builder.create_ref(p_render_data);
    result->update_buffer(p_values.data(), buffer_size);
    return result;
}

std::string_view get_vertex_stage_entry_name(RefRes<RUGraphicsPipeline> p_pipeline) {
    if (p_pipeline == nullptr || p_pipeline->vertex_stage == nullptr) {
        throw std::runtime_error("Failed to construct static set task: vertex shader stage is missing.");
    }
    if (p_pipeline->vertex_stage_entry != nullptr && p_pipeline->vertex_stage_entry.get()[0] != '\0') {
        return p_pipeline->vertex_stage_entry.get();
    }
    if (p_pipeline->vertex_stage->vertex_entry != nullptr && p_pipeline->vertex_stage->vertex_entry.get()[0] != '\0') {
        return p_pipeline->vertex_stage->vertex_entry.get();
    }
    return {};
}

RefRes<RUMaterial> get_default_material(std::string_view p_default_material_key) {
    ResourceManager* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr || p_default_material_key.empty()) {
        return MEM_NULL;
    }
    return resource_manager->get_resource<RUMaterial>(std::string(p_default_material_key));
}

RefRes<RUTexture> get_default_image(std::string_view p_default_material_key) {
    RefRes<RUMaterial> material = get_default_material(p_default_material_key);
    if (material == nullptr || material->texture_count == 0 || material->textures == nullptr) {
        return MEM_NULL;
    }
    return material->textures.get()[0].texture;
}

std::vector<VkFormat> get_color_attachment_formats(const RTRunStaticSetVK::Config& p_config, const BaseRenderDataVK* p_render_data) {
    std::vector<VkFormat> result;
    result.reserve(p_config.get_color_attachment_count());
    for (size_t i = 0; i < p_config.get_color_attachment_count(); ++i) {
        result.push_back(p_config.get_color_attachment_format(i));
    }
    if (result.empty()) {
        result.push_back(p_render_data->swap_chain_format);
    }
    return result;
}

std::string pipeline_resource_key(RefRes<RUGraphicsPipeline> p_pipeline) {
    if (p_pipeline != nullptr && p_pipeline->resource_key != nullptr) {
        return p_pipeline->resource_key;
    }
    return "static_set_explicit_pipeline";
}

std::optional<MaterialTextureContract> load_material_contract(RefRes<RUGraphicsPipeline> p_graphics_pipeline,
    std::string_view p_material_textures_variable_name,
    std::string_view p_material_params_variable_name) {
    ShaderMetadata vertex_metadata = ShaderMetadataLoader().load(p_graphics_pipeline->vertex_stage);
    std::optional<ShaderMetadata> geometry_metadata;
    if (p_graphics_pipeline->geometry_stage != nullptr) {
        geometry_metadata = ShaderMetadataLoader().load(p_graphics_pipeline->geometry_stage);
    }
    ShaderMetadata fragment_metadata = ShaderMetadataLoader().load(p_graphics_pipeline->fragment_stage);
    return MaterialTextureContractResolver().resolve(vertex_metadata,
        geometry_metadata.has_value() ? &*geometry_metadata : nullptr,
        fragment_metadata,
        p_material_textures_variable_name,
        p_material_params_variable_name);
}

void validate_static_set_node_layout(const ShaderMetadata& p_vertex_metadata) {
    StaticSetNodeContract reflected_contract = StaticSetNodeContractResolver().resolve(p_vertex_metadata, "static_set");
    StaticSetNodeContract cpu_contract;
    cpu_contract.element_stride = sizeof(StaticSetNodeGPU);
    cpu_contract.transform_offset = offsetof(StaticSetNodeGPU, transform);
    cpu_contract.normal_transform_offset = offsetof(StaticSetNodeGPU, normal_transform);
    if (cpu_contract.matches(reflected_contract)) {
        return;
    }
    throw std::runtime_error(std::format("Static-set node layout mismatch: shader expects stride={}, transform offset={}, normal_transform offset={}, but CPU uploads stride={}, transform offset={}, normal_transform offset={}.",
        reflected_contract.element_stride,
        reflected_contract.transform_offset,
        reflected_contract.normal_transform_offset,
        cpu_contract.element_stride,
        cpu_contract.transform_offset,
        cpu_contract.normal_transform_offset));
}

Ref<ROSamplerVK> create_material_sampler(const BaseRenderDataVK* p_render_data, bool p_has_material_contract) {
    if (!p_has_material_contract) {
        return MEM_NULL;
    }
    uint32_t sampler_index = p_render_data->descriptor_heap_allocator->allocate_sampler();
    return ROSamplerVK::Builder().set_sampler_index(sampler_index).create_ref(p_render_data);
}

RenderBindpointDeclarations build_static_set_bindpoints(
    Ref<ROBufferVK> p_submesh_buffer, Ref<ROBufferVK> p_node_buffer, Ref<ROBufferVK> p_instance_buffer) {
    return RenderBindpointDeclarationsBuilder()
        .add_storage_buffer_decleration("static_set.submeshes", p_submesh_buffer)
        .add_storage_buffer_decleration("static_set.nodes", p_node_buffer)
        .add_storage_buffer_decleration("static_set.instances", p_instance_buffer)
        .build();
}

uint32_t get_material_texture_view_index(GraphicsPipelineCompilerVK& p_pipeline_compiler,
    std::vector<std::pair<RefRes<RUTexture>, Ref<ROImageViewVK>>>& p_material_texture_views,
    RefRes<RUTexture> p_texture) {
    for (const auto& [texture, image_view] : p_material_texture_views) {
        if (texture == p_texture) {
            return image_view->get_sampled_view_index();
        }
    }
    Ref<ROImageViewVK> image_view = p_pipeline_compiler.compile_texture_view(
        std::format("texture_resource_{}", reinterpret_cast<uintptr_t>(p_texture.get())), p_texture);
    uint32_t sampled_view_index = image_view->get_sampled_view_index();
    p_material_texture_views.emplace_back(p_texture, std::move(image_view));
    return sampled_view_index;
}

void resolve_material_image_render_objects(RenderBindpointDeclarations& p_material_bindpoints,
    const std::vector<std::pair<RefRes<RUTexture>, Ref<ROImageViewVK>>>& p_material_texture_views) {
    for (RenderBindpointImageDeclaration& declaration : p_material_bindpoints.image_declerations) {
        for (const auto& [texture, image_view] : p_material_texture_views) {
            if (texture == declaration.image_resource) {
                declaration.image_render_object = image_view;
                declaration.image_resource = MEM_NULL;
                break;
            }
        }
        if (declaration.image_render_object == MEM_NULL) {
            throw std::runtime_error("Failed to compile static-set material texture: resolved image view is missing.");
        }
    }
}

void assign_material_buffer_render_objects(const BaseRenderDataVK* p_render_data, RenderBindpointDeclarations& p_material_bindpoints) {
    for (RenderBindpointBufferDeclaration& declaration : p_material_bindpoints.buffer_declerations) {
        if (declaration.buffer_render_object != MEM_NULL) {
            continue;
        }
        std::span<const uint8_t> buffer_data = p_material_bindpoints.get_buffer_data(declaration);
        uint32_t uniform_view_index = p_render_data->descriptor_heap_allocator->allocate_buffer();
        Ref<ROBufferVK> material_params_buffer = ROBufferVK::Builder()
                                                     .set_domain(DeviceDomain::HOST_DEVICE)
                                                     .set_size(buffer_data.size())
                                                     .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                                     .set_uniform_view_index(uniform_view_index)
                                                     .create_ref(p_render_data);
        material_params_buffer->update_buffer(buffer_data.data(), buffer_data.size());
        declaration.buffer_render_object = material_params_buffer;
        declaration.offset = 0;
        declaration.size = std::numeric_limits<size_t>::max();
    }
}

void append_material_bindpoints(RenderBindpointDeclarations& p_bindpoints, RenderBindpointDeclarations& p_material_bindpoints) {
    if (!p_material_bindpoints.buffer_data.empty()) {
        const size_t buffer_data_offset = p_bindpoints.buffer_data.size();
        p_bindpoints.buffer_data.insert(
            p_bindpoints.buffer_data.end(), p_material_bindpoints.buffer_data.begin(), p_material_bindpoints.buffer_data.end());
        for (RenderBindpointBufferDeclaration& declaration : p_material_bindpoints.buffer_declerations) {
            if (declaration.buffer_render_object == MEM_NULL) {
                declaration.offset += buffer_data_offset;
            }
            p_bindpoints.buffer_declerations.push_back(std::move(declaration));
        }
    } else {
        for (RenderBindpointBufferDeclaration& declaration : p_material_bindpoints.buffer_declerations) {
            p_bindpoints.buffer_declerations.push_back(std::move(declaration));
        }
    }
    for (RenderBindpointImageDeclaration& declaration : p_material_bindpoints.image_declerations) {
        p_bindpoints.image_declerations.push_back(std::move(declaration));
    }
    for (RenderBindpointSamplerDeclaration& declaration : p_material_bindpoints.sampler_declerations) {
        p_bindpoints.sampler_declerations.push_back(std::move(declaration));
    }
}

void validate_submesh_materials(RefRes<RUStaticGeometrySet> p_set) {
    for (size_t i = 0; i < p_set->submesh_count; ++i) {
        if (p_set->submeshes.get()[i].material == nullptr) {
            throw std::runtime_error("Failed to construct static set task: submesh material is missing.");
        }
    }
}

StaticSetBufferCompilerVK::BuildResult build_geometry_buffers(const BaseRenderDataVK* p_render_data, RefRes<RUStaticGeometrySet> p_set) {
    return StaticSetBufferCompilerVK(p_render_data).build(p_set, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

std::optional<uint32_t> allocate_storage_descriptor_range(
    const BaseRenderDataVK* p_render_data, const StaticInstanceBufferData& p_instance_data) {
    uint32_t storage_descriptor_count = static_cast<uint32_t>(!p_instance_data.submeshes.empty()) +
                                        static_cast<uint32_t>(!p_instance_data.nodes.empty()) +
                                        static_cast<uint32_t>(!p_instance_data.instances.empty());
    if (storage_descriptor_count == 0) {
        return std::nullopt;
    }
    return p_render_data->descriptor_heap_allocator->allocate_buffer(storage_descriptor_count);
}

StaticSetStorageBuffers build_static_set_storage_buffers(const BaseRenderDataVK* p_render_data, RefRes<RUStaticGeometrySet> p_set) {
    StaticSetStorageBuffers result;
    result.instance_data = build_static_instance_buffer_data(p_set);
    std::optional<uint32_t> next_storage_descriptor_index = allocate_storage_descriptor_range(p_render_data, result.instance_data);
    auto take_storage_descriptor_index = [&next_storage_descriptor_index](bool p_has_buffer) -> std::optional<uint32_t> {
        if (!p_has_buffer || !next_storage_descriptor_index.has_value()) {
            return std::nullopt;
        }
        return (*next_storage_descriptor_index)++;
    };
    result.submesh_buffer = build_storage_buffer(
        p_render_data, result.instance_data.submeshes, take_storage_descriptor_index(!result.instance_data.submeshes.empty()));
    result.node_buffer = build_storage_buffer(
        p_render_data, result.instance_data.nodes, take_storage_descriptor_index(!result.instance_data.nodes.empty()));
    result.instance_buffer = build_storage_buffer(
        p_render_data, result.instance_data.instances, take_storage_descriptor_index(!result.instance_data.instances.empty()));
    return result;
}

IndirectDrawPlanData build_indirect_draw_plan_data(const BaseRenderDataVK* p_render_data,
    RefRes<RUGraphicsPipeline> p_graphics_pipeline,
    RefRes<RUStaticGeometrySet> p_set,
    const StaticInstanceBufferData& p_instance_data) {
    IndirectDrawPlanData result(p_render_data->allocator);
    ShaderMetadata vertex_metadata = ShaderMetadataLoader().load(p_graphics_pipeline->vertex_stage);
    validate_static_set_node_layout(vertex_metadata);
    const std::string_view vertex_entry_name = get_vertex_stage_entry_name(p_graphics_pipeline);
    result.draw_plan = StaticSetDrawBatchPlannerVK().build(p_render_data->allocator,
        std::span<const RUStaticGeometrySubmesh>(p_set->submeshes.get(), p_set->submesh_count),
        p_instance_data.first_instances,
        vertex_metadata,
        vertex_entry_name,
        p_render_data->max_draw_indirect_count);
    result.pipeline_batches.reserve(result.draw_plan.batches.size());
    return result;
}

void fill_pipeline_batches(
    IndirectDrawPlanData& p_draw_plan_data, const BaseRenderDataVK* p_render_data, Ref<ROGraphicsPipelineVK> p_compiled_graphics_pipeline) {
    for (auto& batch : p_draw_plan_data.draw_plan.batches) {
        RTRunGraphicsPipelineIndirectVK::PipelineBatch pipeline_batch(p_render_data->allocator);
        pipeline_batch.graphics_pipeline = p_compiled_graphics_pipeline;
        pipeline_batch.first_draw = batch.first_draw;
        pipeline_batch.draw_count = batch.draw_count;
        pipeline_batch.vertex_input = std::move(batch.vertex_input);
        pipeline_batch.vertex_buffer_offset = batch.vertex_buffer_offset;
        p_draw_plan_data.pipeline_batches.push_back(std::move(pipeline_batch));
    }
}

Unique<RTRunGraphicsPipelineIndirectVK> build_indirect_static_set_task(const BaseRenderDataVK* p_render_data,
    const StaticSetAttachmentDataVK& p_attachment_data,
    const RTRunStaticSetVK::Config& p_config,
    const StaticSetBufferCompilerVK::BuildResult& p_geometry_buffers,
    Ref<ROBufferVK> p_indirect_buffer,
    std::span<const RTRunGraphicsPipelineIndirectVK::PipelineBatch> p_pipeline_batches) {
    std::vector<std::string_view> color_attachment_ids = color_attachment_id_views(p_attachment_data);
    return make_unique<RTRunGraphicsPipelineIndirectVK>(p_render_data->allocator,
        p_render_data,
        std::span<const VkClearValue>(p_attachment_data.clear_values.data(), p_attachment_data.clear_values.size()),
        p_attachment_data.render_extent,
        std::span<const std::string_view>(color_attachment_ids.data(), color_attachment_ids.size()),
        depth_attachment_id_view(p_attachment_data),
        p_geometry_buffers.vertex_buffer,
        p_geometry_buffers.index_buffer,
        p_indirect_buffer,
        p_pipeline_batches,
        p_config.get_color_attachment_load_op(),
        p_config.get_depth_attachment_load_op());
}

uint32_t get_or_append_material_index(std::vector<RefRes<RUMaterial>>& p_materials, RefRes<RUMaterial> p_material) {
    for (size_t i = 0; i < p_materials.size(); ++i) {
        if (p_materials[i] == p_material) {
            return static_cast<uint32_t>(i);
        }
    }
    if (p_materials.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to construct static set task: material table exceeds uint32.");
    }
    p_materials.push_back(p_material);
    return static_cast<uint32_t>(p_materials.size() - 1);
}

StaticInstanceBufferData build_static_instance_buffer_data(RefRes<RUStaticGeometrySet> p_set) {
    StaticInstanceBufferData result;
    result.submeshes.reserve(p_set->submesh_count);
    result.nodes.reserve(p_set->instance_count);
    result.instances.reserve(p_set->instance_count);
    result.first_instances.reserve(p_set->submesh_count);
    result.ordered_materials.reserve(p_set->submesh_count);
    for (size_t i = 0; i < p_set->instance_count; ++i) {
        const glm::mat4& global_transform = p_set->instances.get()[i].global_transform;
        const glm::mat3 transform_3x3 = glm::mat3(global_transform);
        glm::mat4 normal_transform{1.0F};
        if (glm::abs(glm::determinant(transform_3x3)) >= 0.000001F) {
            normal_transform = glm::mat4(glm::inverseTranspose(transform_3x3));
        }
        result.nodes.push_back(StaticSetNodeGPU{.transform = global_transform, .normal_transform = normal_transform});
    }

    for (size_t submesh_index = 0; submesh_index < p_set->submesh_count; ++submesh_index) {
        const RUStaticGeometrySubmesh& submesh = p_set->submeshes.get()[submesh_index];
        if (result.instances.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to construct static set task: instance table exceeds uint32.");
        }
        result.first_instances.push_back(static_cast<uint32_t>(result.instances.size()));
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            continue;
        }
        const uint32_t material_index = get_or_append_material_index(result.ordered_materials, submesh.material);
        result.submeshes.push_back(StaticSetSubmeshGPU{.material_id = material_index});
        const uint32_t drawable_submesh_index = static_cast<uint32_t>(result.submeshes.size() - 1);
        for (size_t instance_index = 0; instance_index < submesh.instance_count; ++instance_index) {
            const size_t node_index = submesh.first_instance + instance_index;
            if (node_index > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error("Failed to construct static set task: node index exceeds uint32.");
            }
            result.instances.push_back(StaticSetInstanceGPU{
                .submesh_index = drawable_submesh_index,
                .node_index = static_cast<uint32_t>(node_index),
            });
        }
    }
    return result;
}

GraphicsPipelineCompilerVK::CompileResult compile_static_set_pipeline(RendererVK* p_renderer,
    RefRes<RUGraphicsPipeline> p_graphics_pipeline,
    const RTRunStaticSetVK::Config& p_config,
    std::span<const RefRes<RUMaterial>> p_materials,
    Ref<ROBufferVK> p_submesh_buffer,
    Ref<ROBufferVK> p_node_buffer,
    Ref<ROBufferVK> p_instance_buffer) {
    const BaseRenderDataVK* render_data = p_renderer->get_base_render_data();
    const std::optional<MaterialTextureContract> material_contract = load_material_contract(
        p_graphics_pipeline, p_config.get_material_textures_variable_name(), p_config.get_material_params_variable_name());
    Ref<ROSamplerVK> material_sampler = create_material_sampler(render_data, material_contract.has_value());
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_data->allocator,
        .render_data = render_data,
        .render_database = p_renderer->get_render_database(),
        .renderer = p_renderer,
        .default_texture_resource = get_default_image(p_config.get_default_material_resource_key()),
        .color_attachment_formats = get_color_attachment_formats(p_config, render_data),
        .depth_attachment_format = p_config.get_depth_attachment_format(),
        .stencil_attachment_format = p_config.get_stencil_attachment_format(),
    };
    GraphicsPipelineCompilerVK pipeline_compiler(compiler_context);
    std::vector<std::pair<RefRes<RUTexture>, Ref<ROImageViewVK>>> material_texture_views;
    const std::string compiled_pipeline_key = std::format("{}#static_set", pipeline_resource_key(p_graphics_pipeline));
    RenderBindpointDeclarations bindpoints = build_static_set_bindpoints(p_submesh_buffer, p_node_buffer, p_instance_buffer);
    RenderBindpointDeclarations material_bindpoints = StaticSetMaterialBindingBuilderVK().build(material_contract,
        p_materials,
        get_default_material(p_config.get_default_material_resource_key()),
        p_config.get_material_textures_variable_name(),
        p_config.get_material_params_variable_name(),
        DescriptorHeapAllocatorVK::SAMPLED_IMAGE_CAPACITY,
        material_sampler,
        [&pipeline_compiler, &material_texture_views](RefRes<RUTexture> p_texture) {
            return get_material_texture_view_index(pipeline_compiler, material_texture_views, p_texture);
        });
    resolve_material_image_render_objects(material_bindpoints, material_texture_views);
    assign_material_buffer_render_objects(render_data, material_bindpoints);
    append_material_bindpoints(bindpoints, material_bindpoints);
    return pipeline_compiler.compile(compiled_pipeline_key, p_graphics_pipeline, bindpoints);
}

std::vector<std::string_view> color_attachment_id_views(const StaticSetAttachmentDataVK& p_attachment_data) {
    std::vector<std::string_view> result;
    result.reserve(p_attachment_data.color_attachment_ids.size());
    for (const std::string& color_attachment_id : p_attachment_data.color_attachment_ids) {
        result.push_back(color_attachment_id);
    }
    return result;
}

std::optional<std::string_view> depth_attachment_id_view(const StaticSetAttachmentDataVK& p_attachment_data) {
    if (p_attachment_data.depth_attachment_id.has_value()) {
        return *p_attachment_data.depth_attachment_id;
    }
    return std::nullopt;
}

NonEmptyStaticSetTaskBuild build_non_empty_indirect_task(RendererVK* p_renderer,
    RefRes<RUStaticGeometrySet> p_set,
    RefRes<RUGraphicsPipeline> p_graphics_pipeline,
    const RTRunStaticSetVK::Config& p_config,
    const StaticSetAttachmentDataVK& p_attachment_data) {
    NonEmptyStaticSetTaskBuild result;
    const BaseRenderDataVK* render_data = p_renderer->get_base_render_data();
    validate_submesh_materials(p_set);
    StaticSetBufferCompilerVK::BuildResult geometry_buffers = build_geometry_buffers(render_data, p_set);
    if (geometry_buffers.vertex_buffer == MEM_NULL || geometry_buffers.index_buffer == MEM_NULL) {
        return result;
    }
    StaticSetStorageBuffers storage_buffers = build_static_set_storage_buffers(render_data, p_set);
    GraphicsPipelineCompilerVK::CompileResult compiled_pipeline = compile_static_set_pipeline(p_renderer,
        p_graphics_pipeline,
        p_config,
        storage_buffers.instance_data.ordered_materials,
        storage_buffers.submesh_buffer,
        storage_buffers.node_buffer,
        storage_buffers.instance_buffer);
    result.graphics_pipeline = compiled_pipeline.graphics_pipeline;
    IndirectDrawPlanData draw_plan_data =
        build_indirect_draw_plan_data(render_data, p_graphics_pipeline, p_set, storage_buffers.instance_data);
    fill_pipeline_batches(draw_plan_data, render_data, compiled_pipeline.graphics_pipeline);
    Ref<ROBufferVK> indirect_buffer = build_indirect_buffer(render_data, draw_plan_data.draw_plan.commands);
    if (indirect_buffer == nullptr) {
        return result;
    }
    result.indirect_task = build_indirect_static_set_task(render_data,
        p_attachment_data,
        p_config,
        geometry_buffers,
        indirect_buffer,
        std::span<const RTRunGraphicsPipelineIndirectVK::PipelineBatch>(
            draw_plan_data.pipeline_batches.data(), draw_plan_data.pipeline_batches.size()));
    return result;
}

} // namespace

RTRunStaticSetVK::RTRunStaticSetVK(
    RendererVK* p_renderer, RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_graphics_pipeline, const Config& p_config)
    : render_data(p_renderer != nullptr ? p_renderer->get_base_render_data() : nullptr), geometry_set(p_set),
      graphics_pipeline(p_graphics_pipeline), config(p_config),
      color_attachment_containers(p_renderer != nullptr ? p_renderer->get_base_render_data()->allocator : nullptr) {
    validate_constructor_inputs(p_renderer, p_set, p_graphics_pipeline);
    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    if (!is_empty_set(*geometry_set)) {
        NonEmptyStaticSetTaskBuild built_task =
            build_non_empty_indirect_task(p_renderer, geometry_set, graphics_pipeline, config, attachment_data);
        indirect_task = std::move(built_task.indirect_task);
        compiled_graphics_pipeline = std::move(built_task.graphics_pipeline);
    }
}

QueueFamilyVK RTRunStaticSetVK::queue_family() const {
    return QueueFamilyVK::GRAPHICS;
}

void RTRunStaticSetVK::bind_resources(const RendResViewVK& p_resources) {
    if (!indirect_task.is_null()) {
        indirect_task->bind_resources(p_resources);
        return;
    }
    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    color_attachment_containers.clear();
    color_attachment_containers.reserve(attachment_data.color_attachment_ids.size());
    for (const std::string& color_attachment_id : attachment_data.color_attachment_ids) {
        color_attachment_containers.push_back(p_resources.get(color_attachment_id).container);
    }
    if (attachment_data.depth_attachment_id.has_value()) {
        depth_attachment_container = p_resources.get(*attachment_data.depth_attachment_id).container;
    }
}

Vector<ResAccessVK, RenderAllocator> RTRunStaticSetVK::read_resources() const {
    if (!indirect_task.is_null()) {
        return indirect_task->read_resources();
    }
    Vector<ResAccessVK, RenderAllocator> read_accesses(render_data->allocator);
    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    if (config.get_color_attachment_load_op() == VK_ATTACHMENT_LOAD_OP_LOAD) {
        for (const std::string& color_attachment_id : attachment_data.color_attachment_ids) {
            read_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
        }
    }
    if (attachment_data.depth_attachment_id.has_value() && config.get_depth_attachment_load_op() == VK_ATTACHMENT_LOAD_OP_LOAD) {
        read_accesses.push_back({.res_id = *attachment_data.depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    return read_accesses;
}

Vector<ResAccessVK, RenderAllocator> RTRunStaticSetVK::write_resources() const {
    if (!indirect_task.is_null()) {
        return indirect_task->write_resources();
    }
    Vector<ResAccessVK, RenderAllocator> write_accesses(render_data->allocator);
    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    write_accesses.reserve(attachment_data.color_attachment_ids.size() + (attachment_data.depth_attachment_id.has_value() ? 1 : 0));
    for (const std::string& color_attachment_id : attachment_data.color_attachment_ids) {
        write_accesses.push_back({.res_id = color_attachment_id, .usage = RendResUsageVK::COLOR_ATTACHMENT});
    }
    if (attachment_data.depth_attachment_id.has_value()) {
        write_accesses.push_back({.res_id = *attachment_data.depth_attachment_id, .usage = RendResUsageVK::DEPTH_ATTACHMENT});
    }
    return write_accesses;
}

void RTRunStaticSetVK::record(VkCommandBuffer p_command_buffer, double /*p_delta*/) {
    if (!indirect_task.is_null()) {
        indirect_task->record(p_command_buffer, 0.0);
        return;
    }

    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    Vector<VkRenderingAttachmentInfo, RenderAllocator> color_attachments(render_data->allocator);
    color_attachments.reserve(attachment_data.color_attachment_ids.size());
    for (size_t i = 0; i < attachment_data.color_attachment_ids.size(); ++i) {
        Ref<ROImageViewVK> color_attachment =
            WBE_REQUIRES_VALID(color_attachment_containers[i]->get().dynamic_cast_ref<ROImageViewVK>());
        color_attachments.push_back(ROImageViewVK::AttachmentBuilder::color(color_attachment->get_image_view())
                .set_load_op(config.get_color_attachment_load_op())
                .set_clear_value(attachment_data.clear_values[i])
                .build());
    }

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = {.offset = {.x = 0, .y = 0}, .extent = attachment_data.render_extent};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
    rendering_info.pColorAttachments = color_attachments.data();

    VkRenderingAttachmentInfo depth_attachment_info{};
    if (attachment_data.depth_attachment_id.has_value()) {
        Ref<ROImageViewVK> depth_attachment =
            WBE_REQUIRES_VALID(depth_attachment_container->get().dynamic_cast_ref<ROImageViewVK>());
        depth_attachment_info = ROImageViewVK::AttachmentBuilder::depth(depth_attachment->get_image_view())
                                    .set_load_op(config.get_depth_attachment_load_op())
                                    .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
                                    .set_clear_value(attachment_data.clear_values[color_attachments.size()])
                                    .build();
        rendering_info.pDepthAttachment = &depth_attachment_info;
    }

    vkCmdBeginRendering(p_command_buffer, &rendering_info);

    VkViewport viewport = render_data->viewport;
    viewport.width = static_cast<float>(attachment_data.render_extent.width);
    viewport.height = static_cast<float>(attachment_data.render_extent.height);
    vkCmdSetViewport(p_command_buffer, 0, 1, &viewport);
    VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = attachment_data.render_extent};
    vkCmdSetScissor(p_command_buffer, 0, 1, &scissor);

    vkCmdEndRendering(p_command_buffer);
}

void RTRunStaticSetVK::set_descriptor_binding(std::string_view p_field_path, Ref<IRenderObject> p_resource) {
    if (compiled_graphics_pipeline == MEM_NULL) {
        throw std::runtime_error("Failed to set static-set descriptor binding: compiled graphics pipeline is unavailable.");
    }
    compiled_graphics_pipeline->set_descriptor_binding(p_field_path, std::move(p_resource));
}

void RTRunStaticSetVK::set_descriptor_binding(std::string_view p_field_path, Ref<RenderObjectContainerVK> p_resource_container) {
    if (compiled_graphics_pipeline == MEM_NULL) {
        throw std::runtime_error("Failed to set static-set descriptor binding: compiled graphics pipeline is unavailable.");
    }
    compiled_graphics_pipeline->set_descriptor_binding(p_field_path, std::move(p_resource_container));
}

} // namespace WhiteBirdEngine
