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
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <glm/ext/matrix_float4x4.hpp>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
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
};

struct NonEmptyStaticSetTaskBuild {
    Unique<RTRunGraphicsPipelineIndirectVK> indirect_task = MEM_NULL;
    Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
    GraphicsPipelineCompilerVK::DescriptorSetBindings descriptor_set_bindings;
};

struct StaticSetDrawBinaries {
    RefRes<RUBinary> vertex_binary = MEM_NULL;
    RefRes<RUBinary> index_binary = MEM_NULL;
};

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

void validate_attachment_resource(std::string_view p_resource_id, const RendResViewVK& p_resources) {
    Ref<ROImageViewVK> image_view = p_resources.get(p_resource_id).container->get().dynamic_cast_ref<ROImageViewVK>();
    if (image_view == MEM_NULL) {
        throw std::runtime_error("RTRunStaticSetVK: attachment resource has the wrong type.");
    }
}

bool is_empty_set(const RUStaticGeometrySet& p_set) {
    return p_set.submesh_count == 0 || p_set.instance_count == 0;
}

void validate_binary_view(const RUBinaryView& p_view, std::string_view p_context) {
    if (p_view.binary == nullptr) {
        throw std::runtime_error(std::format("Failed to construct static set task: {} binary is missing.", p_context));
    }
    if (p_view.start > p_view.binary->size || p_view.size > p_view.binary->size - p_view.start) {
        throw std::runtime_error(std::format("Failed to construct static set task: {} binary view exceeds binary size.", p_context));
    }
    if (p_view.size > 0 && p_view.binary->data == nullptr) {
        throw std::runtime_error(std::format("Failed to construct static set task: {} binary data is missing.", p_context));
    }
}

StaticSetDrawBinaries validate_static_set_and_get_binaries(const RUStaticGeometrySet& p_set) {
    if (p_set.submesh_count > 0 && p_set.submeshes == nullptr) {
        throw std::runtime_error("Failed to construct static set task: submesh storage is missing.");
    }
    if (p_set.instance_count > 0 && p_set.instances == nullptr) {
        throw std::runtime_error("Failed to construct static set task: instance storage is missing.");
    }
    StaticSetDrawBinaries binaries;
    for (size_t i = 0; i < p_set.submesh_count; ++i) {
        const RUStaticGeometrySubmesh& submesh = p_set.submeshes.get()[i];
        if (submesh.material == nullptr) {
            throw std::runtime_error("Failed to construct static set task: submesh material is missing.");
        }
        validate_binary_view(submesh.vertices.binary, "vertex");
        validate_binary_view(submesh.indices.binary, "index");
        if (submesh.vertices.vertex_stride == 0 ||
            submesh.vertices.binary.size != submesh.vertices.vertex_count * submesh.vertices.vertex_stride) {
            throw std::runtime_error("Failed to construct static set task: vertex view count, stride, and size do not match.");
        }
        if (submesh.indices.binary.start % sizeof(uint32_t) != 0 ||
            submesh.indices.binary.size != submesh.indices.index_count * sizeof(uint32_t)) {
            throw std::runtime_error("Failed to construct static set task: index view is not a uint32 range.");
        }
        if (submesh.first_instance > p_set.instance_count || submesh.instance_count > p_set.instance_count - submesh.first_instance) {
            throw std::runtime_error("Failed to construct static set task: submesh instance range is out of bounds.");
        }
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            continue;
        }
        if (binaries.vertex_binary == nullptr) {
            binaries.vertex_binary = submesh.vertices.binary.binary;
        }
        if (binaries.index_binary == nullptr) {
            binaries.index_binary = submesh.indices.binary.binary;
        }
        if (submesh.vertices.binary.binary != binaries.vertex_binary) {
            throw std::runtime_error("Failed to construct static set task: all drawable submeshes must share one vertex binary resource.");
        }
        if (submesh.indices.binary.binary != binaries.index_binary) {
            throw std::runtime_error("Failed to construct static set task: all drawable submeshes must share one index binary resource.");
        }
    }
    return binaries;
}

Ref<ROBufferVK> build_static_binary_buffer(
    const BaseRenderDataVK* p_render_data, RefRes<RUBinary> p_binary, VkBufferUsageFlags p_usage, std::string_view p_resource_kind) {
    if (p_binary == nullptr || p_binary->data == nullptr || p_binary->size == 0) {
        throw std::runtime_error(std::format("Failed to construct static set task: {} binary data is missing.", p_resource_kind));
    }
    if (p_binary->resource_key == nullptr || p_binary->resource_key[0] == '\0') {
        throw std::runtime_error(std::format("Failed to construct static set task: {} binary resource key is missing.", p_resource_kind));
    }
    if (p_render_data->render_database == nullptr) {
        throw std::runtime_error("Failed to construct static set task: render database is missing.");
    }

    Ref<IRenderObject> registered_buffer = p_render_data->render_database->get_compiled_render_resource(
        p_binary->resource_key, TypeUUID<ROBufferVK>::UUID);
    if (registered_buffer != MEM_NULL) {
        Ref<ROBufferVK> result = registered_buffer.dynamic_cast_ref<ROBufferVK>();
        if (result == MEM_NULL) {
            throw std::runtime_error("Failed to construct static set task: registered geometry buffer has the wrong type.");
        }
        if (result->fields().buffer_size != p_binary->size) {
            throw std::runtime_error("Failed to construct static set task: registered geometry buffer size does not match its binary resource.");
        }
        return result;
    }

    Ref<ROBufferVK> result = ROBufferVK::Builder()
                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                 .set_size(p_binary->size)
                                 .set_usage(p_usage)
                                 .create_ref(p_render_data);
    result->update_buffer(p_binary->data.get(), p_binary->size);
    p_render_data->render_database->register_compiled_render_resource(p_binary->resource_key, result);
    return result;
}

Ref<ROBufferVK> build_indirect_buffer(const BaseRenderDataVK* p_render_data, const std::vector<VkDrawIndexedIndirectCommand>& p_commands) {
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
Ref<ROBufferVK> build_storage_buffer(const BaseRenderDataVK* p_render_data, const std::vector<TValue>& p_values) {
    if (p_values.empty()) {
        return MEM_NULL;
    }
    const size_t buffer_size = p_values.size() * sizeof(TValue);
    Ref<ROBufferVK> result = ROBufferVK::Builder()
                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                 .set_size(buffer_size)
                                 .set_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                                 .create_ref(p_render_data);
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

RefRes<RUTexture> get_default_image(std::string_view p_default_material_key) {
    ResourceManager* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr || p_default_material_key.empty()) {
        return MEM_NULL;
    }
    RefRes<RUMaterial> material = resource_manager->get_resource<RUMaterial>(std::string(p_default_material_key));
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
    for (size_t i = 0; i < p_set->instance_count; ++i) {
        result.nodes.push_back(StaticSetNodeGPU{.transform = p_set->instances.get()[i].global_transform});
    }

    std::vector<RefRes<RUMaterial>> materials;
    materials.reserve(p_set->submesh_count);
    for (size_t submesh_index = 0; submesh_index < p_set->submesh_count; ++submesh_index) {
        const RUStaticGeometrySubmesh& submesh = p_set->submeshes.get()[submesh_index];
        if (result.instances.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Failed to construct static set task: instance table exceeds uint32.");
        }
        result.first_instances.push_back(static_cast<uint32_t>(result.instances.size()));
        if (submesh.indices.index_count == 0 || submesh.instance_count == 0) {
            continue;
        }
        const uint32_t material_index = get_or_append_material_index(materials, submesh.material);
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
    Ref<ROBufferVK> p_submesh_buffer,
    Ref<ROBufferVK> p_node_buffer,
    Ref<ROBufferVK> p_instance_buffer) {
    const BaseRenderDataVK* render_data = p_renderer->get_base_render_data();
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
    const std::string compiled_pipeline_key = std::format("{}#static_set", pipeline_resource_key(p_graphics_pipeline));
    RenderBindpointDeclarations bindpoints = RenderBindpointDeclarationsBuilder()
                                                 .add_storage_buffer_decleration("static_set.submeshes", p_submesh_buffer)
                                                 .add_storage_buffer_decleration("static_set.nodes", p_node_buffer)
                                                 .add_storage_buffer_decleration("static_set.instances", p_instance_buffer)
                                                 .build();
    return GraphicsPipelineCompilerVK(compiler_context).compile(compiled_pipeline_key, p_graphics_pipeline, bindpoints);
}

void validate_drawable_static_submesh(const RUStaticGeometrySubmesh& p_submesh) {
    if (p_submesh.indices.index_count > std::numeric_limits<uint32_t>::max() ||
        p_submesh.indices.binary.start / sizeof(uint32_t) > std::numeric_limits<uint32_t>::max() ||
        p_submesh.instance_count > std::numeric_limits<uint32_t>::max() ||
        p_submesh.first_instance > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to construct static set task: draw command exceeds Vulkan uint32 limits.");
    }
}

void append_static_submesh_draw(const RUStaticGeometrySubmesh& p_submesh,
    uint32_t p_first_instance,
    Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry_name,
    RenderAllocator* p_allocator,
    std::vector<VkDrawIndexedIndirectCommand>& p_indirect_commands,
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch>& p_pipeline_batches) {
    if (p_submesh.indices.index_count == 0 || p_submesh.instance_count == 0) {
        return;
    }
    validate_drawable_static_submesh(p_submesh);
    p_indirect_commands.push_back(VkDrawIndexedIndirectCommand{
        .indexCount = static_cast<uint32_t>(p_submesh.indices.index_count),
        .instanceCount = static_cast<uint32_t>(p_submesh.instance_count),
        .firstIndex = static_cast<uint32_t>(p_submesh.indices.binary.start / sizeof(uint32_t)),
        .vertexOffset = 0,
        .firstInstance = p_first_instance,
    });
    RTRunGraphicsPipelineIndirectVK::PipelineBatch pipeline_batch(p_allocator);
    pipeline_batch.graphics_pipeline = p_graphics_pipeline;
    pipeline_batch.first_draw = static_cast<uint32_t>(p_indirect_commands.size() - 1);
    pipeline_batch.draw_count = 1;
    pipeline_batch.vertex_input = build_vertex_input_description_vk(p_vertex_metadata, p_vertex_entry_name, p_submesh.vertices);
    pipeline_batch.vertex_buffer_offset = p_submesh.vertices.binary.start;
    p_pipeline_batches.push_back(std::move(pipeline_batch));
}

void build_static_set_draw_data(RefRes<RUStaticGeometrySet> p_set,
    Ref<ROGraphicsPipelineVK> p_graphics_pipeline,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry_name,
    const std::vector<uint32_t>& p_first_instances,
    RenderAllocator* p_allocator,
    std::vector<VkDrawIndexedIndirectCommand>& p_indirect_commands,
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch>& p_pipeline_batches) {
    p_indirect_commands.reserve(p_set->submesh_count);
    p_pipeline_batches.reserve(p_set->submesh_count);
    for (size_t i = 0; i < p_set->submesh_count; ++i) {
        const RUStaticGeometrySubmesh& submesh = p_set->submeshes.get()[i];
        append_static_submesh_draw(
            submesh, p_first_instances[i], p_graphics_pipeline, p_vertex_metadata, p_vertex_entry_name, p_allocator, p_indirect_commands, p_pipeline_batches);
    }
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
    StaticSetDrawBinaries binaries = validate_static_set_and_get_binaries(*p_set);
    if (binaries.vertex_binary == nullptr || binaries.index_binary == nullptr) {
        return result;
    }
    Ref<ROBufferVK> vertex_buffer =
        build_static_binary_buffer(render_data, binaries.vertex_binary, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "vertex");
    Ref<ROBufferVK> index_buffer =
        build_static_binary_buffer(render_data, binaries.index_binary, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "index");
    StaticInstanceBufferData instance_data = build_static_instance_buffer_data(p_set);
    Ref<ROBufferVK> submesh_buffer = build_storage_buffer(render_data, instance_data.submeshes);
    Ref<ROBufferVK> node_buffer = build_storage_buffer(render_data, instance_data.nodes);
    Ref<ROBufferVK> instance_buffer = build_storage_buffer(render_data, instance_data.instances);
    GraphicsPipelineCompilerVK::CompileResult compiled_pipeline =
        compile_static_set_pipeline(p_renderer, p_graphics_pipeline, p_config, submesh_buffer, node_buffer, instance_buffer);
    result.graphics_pipeline = compiled_pipeline.graphics_pipeline;
    result.descriptor_set_bindings = compiled_pipeline.descriptor_set_bindings;
    ShaderMetadata vertex_metadata = ShaderMetadataLoader().load(p_graphics_pipeline->vertex_stage);
    const std::string_view vertex_entry_name = get_vertex_stage_entry_name(p_graphics_pipeline);
    std::vector<VkDrawIndexedIndirectCommand> indirect_commands;
    std::vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch> pipeline_batches;
    build_static_set_draw_data(p_set,
        compiled_pipeline.graphics_pipeline,
        vertex_metadata,
        vertex_entry_name,
        instance_data.first_instances,
        render_data->allocator,
        indirect_commands,
        pipeline_batches);
    Ref<ROBufferVK> indirect_buffer = build_indirect_buffer(render_data, indirect_commands);
    if (indirect_buffer == nullptr) {
        return result;
    }
    std::vector<std::string_view> color_attachment_ids = color_attachment_id_views(p_attachment_data);
    result.indirect_task = make_unique<RTRunGraphicsPipelineIndirectVK>(render_data->allocator,
        render_data,
        std::span<const VkClearValue>(p_attachment_data.clear_values.data(), p_attachment_data.clear_values.size()),
        p_attachment_data.render_extent,
        std::span<const std::string_view>(color_attachment_ids.data(), color_attachment_ids.size()),
        depth_attachment_id_view(p_attachment_data),
        vertex_buffer,
        index_buffer,
        indirect_buffer,
        std::span<const RTRunGraphicsPipelineIndirectVK::PipelineBatch>(pipeline_batches.data(), pipeline_batches.size()),
        p_config.get_color_attachment_load_op(),
        p_config.get_depth_attachment_load_op());
    return result;
}

} // namespace

RTRunStaticSetVK::RTRunStaticSetVK(
    RendererVK* p_renderer, RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_graphics_pipeline, const Config& p_config)
    : render_data(p_renderer != nullptr ? p_renderer->get_base_render_data() : nullptr), geometry_set(p_set),
      graphics_pipeline(p_graphics_pipeline), config(p_config) {
    validate_constructor_inputs(p_renderer, p_set, p_graphics_pipeline);
    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    if (!is_empty_set(*geometry_set)) {
        NonEmptyStaticSetTaskBuild built_task =
            build_non_empty_indirect_task(p_renderer, geometry_set, graphics_pipeline, config, attachment_data);
        indirect_task = std::move(built_task.indirect_task);
        compiled_graphics_pipeline = std::move(built_task.graphics_pipeline);
        descriptor_set_bindings = std::move(built_task.descriptor_set_bindings);
    }
}

QueueFamilyVK RTRunStaticSetVK::queue_family() const {
    return QueueFamilyVK::GRAPHICS;
}

void RTRunStaticSetVK::validate_resources(const RendResViewVK& p_resources) const {
    if (!indirect_task.is_null()) {
        indirect_task->validate_resources(p_resources);
        return;
    }
    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    for (const std::string& color_attachment_id : attachment_data.color_attachment_ids) {
        validate_attachment_resource(color_attachment_id, p_resources);
    }
    if (attachment_data.depth_attachment_id.has_value()) {
        validate_attachment_resource(*attachment_data.depth_attachment_id, p_resources);
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

void RTRunStaticSetVK::record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double /*p_delta*/) {
    if (!indirect_task.is_null()) {
        indirect_task->record(p_command_buffer, p_resources, 0.0);
        return;
    }

    const StaticSetAttachmentDataVK attachment_data = build_attachment_data(config, render_data);
    Vector<VkRenderingAttachmentInfo, RenderAllocator> color_attachments(render_data->allocator);
    color_attachments.reserve(attachment_data.color_attachment_ids.size());
    for (size_t i = 0; i < attachment_data.color_attachment_ids.size(); ++i) {
        Ref<ROImageViewVK> color_attachment = WBE_REQUIRES_VALID(
            p_resources.get(attachment_data.color_attachment_ids[i]).container->get().dynamic_cast_ref<ROImageViewVK>());
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
        Ref<ROImageViewVK> depth_attachment = WBE_REQUIRES_VALID(
            p_resources.get(*attachment_data.depth_attachment_id).container->get().dynamic_cast_ref<ROImageViewVK>());
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

void RTRunStaticSetVK::write_descriptor_set(size_t p_set_index, const RODescriptorSetVK::Writer& p_writer) {
    if (compiled_graphics_pipeline == MEM_NULL) {
        throw std::runtime_error("Failed to write static set task descriptor set: no compiled graphics pipeline is available.");
    }
    compiled_graphics_pipeline->write_descriptor_set(static_cast<uint32_t>(p_set_index), p_writer);
}

RTRunStaticSetVK::DescriptorBinding RTRunStaticSetVK::get_descriptor_binding(std::string_view p_field_path) const {
    auto binding_it = descriptor_set_bindings.find(std::string(p_field_path));
    if (binding_it == descriptor_set_bindings.end()) {
        throw std::runtime_error(
            std::format("Failed to get static set task descriptor binding: field path '{}' was not found.", p_field_path));
    }
    auto [set_index, binding] = binding_it->second;
    return DescriptorBinding{.set_index = set_index, .binding = binding};
}

} // namespace WhiteBirdEngine
