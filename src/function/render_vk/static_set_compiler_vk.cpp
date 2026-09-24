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
#include "function/render_vk/static_set_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "function/function_utils.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/static_set_node_contract.hh"
#include "function/render/static_set_node_contract_resolver.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/static_set_buffer_compiler_vk.hh"
#include "function/render_vk/static_set_draw_batch_planner_vk.hh"
#include "function/render_vk/static_set_material_resource_builder_vk.hh"
#include "function/render_vk/static_set_resources_vk.hh"
#include "glm/common.hpp"
#include "glm/matrix.hpp"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
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

namespace {

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

struct StaticSetStorageBuffers {
    StaticInstanceBufferData instance_data;
    Ref<ROBufferVK> submesh_buffer = MEM_NULL;
    Ref<ROBufferVK> node_buffer = MEM_NULL;
    Ref<ROBufferVK> instance_buffer = MEM_NULL;
};

struct StaticSetResourceInputs {
    bool has_opaque_draws = false;
    bool has_masked_draws = false;
};

struct StaticSetGeometryBuffers {
    StaticSetGeometryBuffers() = default;
    ~StaticSetGeometryBuffers() = default;
    StaticSetGeometryBuffers(const StaticSetGeometryBuffers&) = delete;
    StaticSetGeometryBuffers(StaticSetGeometryBuffers&&) noexcept = default;
    StaticSetGeometryBuffers& operator=(const StaticSetGeometryBuffers&) = delete;
    StaticSetGeometryBuffers& operator=(StaticSetGeometryBuffers&&) noexcept = default;

    Ref<ROBufferVK> vertex_buffer = MEM_NULL;
    Ref<ROBufferVK> index_buffer = MEM_NULL;
};

struct StaticSetDrawBuild {
    explicit StaticSetDrawBuild(RenderAllocator* p_allocator) : batches(p_allocator) {
    }
    ~StaticSetDrawBuild() = default;
    StaticSetDrawBuild(const StaticSetDrawBuild&) = delete;
    StaticSetDrawBuild(StaticSetDrawBuild&&) noexcept = default;
    StaticSetDrawBuild& operator=(const StaticSetDrawBuild&) = delete;
    StaticSetDrawBuild& operator=(StaticSetDrawBuild&&) noexcept = default;

    Ref<ROBufferVK> draw_buffer = MEM_NULL;
    Vector<StaticSetResourcesVK::DrawBatch, RenderAllocator> batches;
};

StaticInstanceBufferData build_static_instance_buffer_data(RefRes<RUStaticGeometrySet> p_set);

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
                                 .set_usage(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
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
    const DescriptorArenaLocationVK arena = p_render_data->sampler_descriptor_arena;
    uint32_t sampler_index = p_render_data->descriptor_heap_allocator->allocate_descriptor(arena.heap_id, arena.arena_id);
    return ROSamplerVK::Builder().set_sampler_index(sampler_index).create_ref(p_render_data);
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

Ref<ROBufferVK> create_material_params_buffer(const BaseRenderDataVK* p_render_data, std::span<const uint8_t> p_buffer_data) {
    const DescriptorArenaLocationVK arena = p_render_data->buffer_descriptor_arena;
    uint32_t uniform_view_index = p_render_data->descriptor_heap_allocator->allocate_descriptor(arena.heap_id, arena.arena_id);
    Ref<ROBufferVK> result = ROBufferVK::Builder()
                                 .set_domain(DeviceDomain::HOST_DEVICE)
                                 .set_size(p_buffer_data.size())
                                 .set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                                 .set_uniform_view_index(uniform_view_index)
                                 .create_ref(p_render_data);
    result->update_buffer(p_buffer_data.data(), p_buffer_data.size());
    return result;
}

void validate_submesh_materials(RefRes<RUStaticGeometrySet> p_set) {
    for (size_t i = 0; i < p_set->submesh_count; ++i) {
        if (p_set->submeshes.get()[i].material == nullptr) {
            throw std::runtime_error("Failed to compile static-set resources: submesh material is missing.");
        }
    }
}

StaticSetBufferCompilerVK::BuildResult build_geometry_buffers(const BaseRenderDataVK* p_render_data, RefRes<RUStaticGeometrySet> p_set) {
    return StaticSetBufferCompilerVK(p_render_data).build(p_set, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

StaticSetStorageBuffers build_static_set_storage_buffers_from_data(
    const BaseRenderDataVK* p_render_data, StaticInstanceBufferData p_instance_data) {
    StaticSetStorageBuffers result;
    result.instance_data = std::move(p_instance_data);
    result.submesh_buffer = build_storage_buffer(p_render_data, result.instance_data.submeshes);
    result.node_buffer = build_storage_buffer(p_render_data, result.instance_data.nodes);
    result.instance_buffer = build_storage_buffer(p_render_data, result.instance_data.instances);
    return result;
}

uint32_t get_or_append_material_index(std::vector<RefRes<RUMaterial>>& p_materials, RefRes<RUMaterial> p_material) {
    for (size_t i = 0; i < p_materials.size(); ++i) {
        if (p_materials[i] == p_material) {
            return static_cast<uint32_t>(i);
        }
    }
    if (p_materials.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to compile static-set resources: material table exceeds uint32.");
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
            throw std::runtime_error("Failed to compile static-set resources: instance table exceeds uint32.");
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
                throw std::runtime_error("Failed to compile static-set resources: node index exceeds uint32.");
            }
            result.instances.push_back(StaticSetInstanceGPU{
                .submesh_index = drawable_submesh_index,
                .node_index = static_cast<uint32_t>(node_index),
            });
        }
    }
    return result;
}

struct CombinedStaticInstanceBufferData {
    StaticInstanceBufferData data;
    std::vector<uint32_t> opaque_first_instances;
    std::vector<uint32_t> masked_first_instances;
};

void append_static_instance_buffer_data(StaticInstanceBufferData& p_destination,
    std::vector<uint32_t>& p_destination_first_instances,
    const StaticInstanceBufferData& p_source) {
    const size_t node_offset = p_destination.nodes.size();
    const size_t submesh_offset = p_destination.submeshes.size();
    const size_t instance_offset = p_destination.instances.size();
    if (node_offset > std::numeric_limits<uint32_t>::max() || submesh_offset > std::numeric_limits<uint32_t>::max() ||
        instance_offset > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to compile static-set resources: combined table offset exceeds uint32.");
    }
    p_destination.nodes.insert(p_destination.nodes.end(), p_source.nodes.begin(), p_source.nodes.end());
    p_destination_first_instances.reserve(p_source.first_instances.size());
    for (uint32_t first_instance : p_source.first_instances) {
        if (first_instance > std::numeric_limits<uint32_t>::max() - instance_offset) {
            throw std::runtime_error("Failed to compile static-set resources: combined first-instance index exceeds uint32.");
        }
        p_destination_first_instances.push_back(static_cast<uint32_t>(instance_offset) + first_instance);
    }
    for (const StaticSetSubmeshGPU& source_submesh : p_source.submeshes) {
        if (source_submesh.material_id >= p_source.ordered_materials.size()) {
            throw std::runtime_error("Failed to compile static-set resources: source material index is out of bounds.");
        }
        p_destination.submeshes.push_back(StaticSetSubmeshGPU{
            .material_id = get_or_append_material_index(
                p_destination.ordered_materials, p_source.ordered_materials[source_submesh.material_id]),
        });
    }
    for (const StaticSetInstanceGPU& source_instance : p_source.instances) {
        if (source_instance.submesh_index > std::numeric_limits<uint32_t>::max() - submesh_offset ||
            source_instance.node_index > std::numeric_limits<uint32_t>::max() - node_offset) {
            throw std::runtime_error("Failed to compile static-set resources: combined instance index exceeds uint32.");
        }
        p_destination.instances.push_back(StaticSetInstanceGPU{
            .submesh_index = static_cast<uint32_t>(submesh_offset) + source_instance.submesh_index,
            .node_index = static_cast<uint32_t>(node_offset) + source_instance.node_index,
        });
    }
}

CombinedStaticInstanceBufferData build_combined_static_instance_buffer_data(
    RefRes<RUStaticOpaqueSet> p_opaque_set, RefRes<RUStaticMaskedSet> p_masked_set) {
    CombinedStaticInstanceBufferData result;
    if (p_opaque_set != MEM_NULL) {
        append_static_instance_buffer_data(result.data, result.opaque_first_instances, build_static_instance_buffer_data(p_opaque_set));
    }
    if (p_masked_set != MEM_NULL) {
        append_static_instance_buffer_data(result.data, result.masked_first_instances, build_static_instance_buffer_data(p_masked_set));
    }
    return result;
}

struct StaticSetMaterialBuild {
    Ref<ROBufferVK> material_params_buffer = MEM_NULL;
    Ref<ROSamplerVK> material_sampler = MEM_NULL;
    Ref<RenderResourceList> texture_list = MEM_NULL;
};

StaticSetMaterialBuild compile_static_set_materials(RendererVK* p_renderer,
    const std::optional<MaterialTextureContract>& p_material_contract,
    const StaticSetCompilerVK::Config& p_config,
    std::span<const RefRes<RUMaterial>> p_materials) {
    const BaseRenderDataVK* render_data = p_renderer->get_base_render_data();
    StaticSetMaterialBuild result;
    result.texture_list = make_ref<RenderResourceList>(render_data->allocator, render_data->allocator);
    result.material_sampler = create_material_sampler(render_data, p_material_contract.has_value());
    if (!p_material_contract.has_value()) {
        return result;
    }
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_data->allocator,
        .render_data = render_data,
        .render_database = p_renderer->get_render_database(),
        .renderer = p_renderer,
        .default_texture_resource = get_default_image(p_config.default_material_resource_key),
    };
    GraphicsPipelineCompilerVK texture_compiler(compiler_context);
    std::vector<std::pair<RefRes<RUTexture>, Ref<ROImageViewVK>>> material_texture_views;
    StaticSetMaterialResourcesVK material_resources = StaticSetMaterialResourceBuilderVK().build(p_material_contract,
        p_materials,
        get_default_material(p_config.default_material_resource_key),
        render_data->sampled_image_descriptor_capacity,
        result.material_sampler,
        [&texture_compiler, &material_texture_views](RefRes<RUTexture> p_texture) {
            return get_material_texture_view_index(texture_compiler, material_texture_views, p_texture);
        });
    if (material_resources.material_params_data.empty()) {
        throw std::runtime_error("Failed to compile static-set resources: material parameter buffer is missing.");
    }
    result.material_params_buffer = create_material_params_buffer(render_data, material_resources.material_params_data);
    for (auto& [texture, image_view] : material_texture_views) {
        static_cast<void>(texture);
        result.texture_list->add_resource(image_view);
    }
    return result;
}

StaticSetResourceInputs validate_static_set_resource_inputs(RendererVK* p_renderer,
    RefRes<RUStaticOpaqueSet> p_opaque_set,
    RefRes<RUStaticMaskedSet> p_masked_set,
    const ShaderMetadata& p_opaque_vertex_metadata,
    const ShaderMetadata& p_masked_vertex_metadata) {
    if (p_renderer == nullptr || p_renderer->get_base_render_data() == nullptr || p_renderer->get_base_render_data()->allocator == nullptr) {
        throw std::runtime_error("Failed to compile static-set resources: renderer state is missing.");
    }
    StaticSetResourceInputs result{
        .has_opaque_draws = p_opaque_set != MEM_NULL && !is_empty_set(*p_opaque_set),
        .has_masked_draws = p_masked_set != MEM_NULL && !is_empty_set(*p_masked_set),
    };
    if (!result.has_opaque_draws && !result.has_masked_draws) {
        throw std::runtime_error("Failed to compile static-set resources: no drawable opaque or masked set was supplied.");
    }
    if (result.has_opaque_draws) {
        validate_submesh_materials(p_opaque_set);
        validate_static_set_node_layout(p_opaque_vertex_metadata);
    }
    if (result.has_masked_draws) {
        validate_submesh_materials(p_masked_set);
        validate_static_set_node_layout(p_masked_vertex_metadata);
    }
    return result;
}

StaticSetGeometryBuffers build_shared_static_set_geometry_buffers(const BaseRenderDataVK* p_render_data,
    RefRes<RUStaticOpaqueSet> p_opaque_set,
    RefRes<RUStaticMaskedSet> p_masked_set,
    const StaticSetResourceInputs& p_inputs) {
    StaticSetBufferCompilerVK::BuildResult opaque_geometry;
    StaticSetBufferCompilerVK::BuildResult masked_geometry;
    if (p_inputs.has_opaque_draws) {
        opaque_geometry = build_geometry_buffers(p_render_data, p_opaque_set);
    }
    if (p_inputs.has_masked_draws) {
        masked_geometry = build_geometry_buffers(p_render_data, p_masked_set);
    }
    if (opaque_geometry.vertex_buffer != MEM_NULL && masked_geometry.vertex_buffer != MEM_NULL &&
        (opaque_geometry.vertex_buffer != masked_geometry.vertex_buffer || opaque_geometry.index_buffer != masked_geometry.index_buffer)) {
        throw std::runtime_error("Failed to compile static-set resources: opaque and masked sets must share vertex and index buffers.");
    }
    StaticSetGeometryBuffers result;
    result.vertex_buffer = opaque_geometry.vertex_buffer != MEM_NULL ? std::move(opaque_geometry.vertex_buffer)
                                                                     : std::move(masked_geometry.vertex_buffer);
    result.index_buffer = opaque_geometry.index_buffer != MEM_NULL ? std::move(opaque_geometry.index_buffer)
                                                                   : std::move(masked_geometry.index_buffer);
    if (result.vertex_buffer == MEM_NULL || result.index_buffer == MEM_NULL) {
        throw std::runtime_error("Failed to compile static-set resources: shared vertex or index buffer is missing.");
    }
    return result;
}

StaticSetDrawBuild compile_static_set_draw_batches(const BaseRenderDataVK* p_render_data,
    RefRes<RUStaticGeometrySet> p_set,
    std::span<const uint32_t> p_first_instances,
    const ShaderMetadata& p_vertex_metadata,
    std::string_view p_vertex_entry) {
    StaticSetDrawBatchPlannerVK::BuildResult draw_plan = StaticSetDrawBatchPlannerVK().build(p_render_data->allocator,
        std::span<const RUStaticGeometrySubmesh>(p_set->submeshes.get(), p_set->submesh_count),
        p_first_instances,
        p_vertex_metadata,
        p_vertex_entry,
        p_render_data->max_draw_indirect_count);
    StaticSetDrawBuild result(p_render_data->allocator);
    result.draw_buffer = build_indirect_buffer(p_render_data, draw_plan.commands);
    result.batches.reserve(draw_plan.batches.size());
    for (StaticSetDrawBatchPlannerVK::Batch& source_batch : draw_plan.batches) {
        StaticSetResourcesVK::DrawBatch draw_batch;
        draw_batch.first_draw = source_batch.first_draw;
        draw_batch.draw_count = source_batch.draw_count;
        draw_batch.vertex_input = std::move(source_batch.vertex_input);
        draw_batch.vertex_buffer_offset = source_batch.vertex_buffer_offset;
        result.batches.push_back(std::move(draw_batch));
    }
    return result;
}

} // namespace

StaticSetCompilerVK::StaticSetCompilerVK(RendererVK* p_renderer) : renderer(p_renderer) {
    if (renderer == nullptr) {
        throw std::runtime_error("Failed to construct static-set compiler: renderer is null.");
    }
}

Ref<StaticSetResourcesVK> StaticSetCompilerVK::compile(RefRes<RUStaticOpaqueSet> p_opaque_set,
    RefRes<RUStaticMaskedSet> p_masked_set,
    const ShaderMetadata& p_opaque_vertex_metadata,
    std::string_view p_opaque_vertex_entry,
    const ShaderMetadata& p_masked_vertex_metadata,
    std::string_view p_masked_vertex_entry,
    const std::optional<MaterialTextureContract>& p_material_contract,
    const Config& p_config) const {
    const StaticSetResourceInputs inputs = validate_static_set_resource_inputs(
        renderer, p_opaque_set, p_masked_set, p_opaque_vertex_metadata, p_masked_vertex_metadata);
    const BaseRenderDataVK* render_data = renderer->get_base_render_data();
    Ref<StaticSetResourcesVK> result = make_ref<StaticSetResourcesVK>(render_data->allocator, render_data->allocator);
    result->material_params_resource_id = p_config.material_params_variable_name;

    StaticSetGeometryBuffers geometry_buffers =
        build_shared_static_set_geometry_buffers(render_data, p_opaque_set, p_masked_set, inputs);
    result->vertices = std::move(geometry_buffers.vertex_buffer);
    result->indices = std::move(geometry_buffers.index_buffer);

    CombinedStaticInstanceBufferData combined_data = build_combined_static_instance_buffer_data(p_opaque_set, p_masked_set);
    StaticSetStorageBuffers storage_buffers = build_static_set_storage_buffers_from_data(render_data, std::move(combined_data.data));
    result->submeshes = std::move(storage_buffers.submesh_buffer);
    result->nodes = std::move(storage_buffers.node_buffer);
    result->instances = std::move(storage_buffers.instance_buffer);

    if (inputs.has_opaque_draws) {
        StaticSetDrawBuild draw_build = compile_static_set_draw_batches(
            render_data, p_opaque_set, combined_data.opaque_first_instances, p_opaque_vertex_metadata, p_opaque_vertex_entry);
        result->opaque_draw_batches = std::move(draw_build.draw_buffer);
        result->opaque_batches = std::move(draw_build.batches);
    }
    if (inputs.has_masked_draws) {
        StaticSetDrawBuild draw_build = compile_static_set_draw_batches(
            render_data, p_masked_set, combined_data.masked_first_instances, p_masked_vertex_metadata, p_masked_vertex_entry);
        result->masked_draw_batches = std::move(draw_build.draw_buffer);
        result->masked_batches = std::move(draw_build.batches);
    }

    StaticSetMaterialBuild material_build = compile_static_set_materials(renderer,
        p_material_contract,
        p_config,
        std::span<const RefRes<RUMaterial>>(
            storage_buffers.instance_data.ordered_materials.data(), storage_buffers.instance_data.ordered_materials.size()));
    result->material_params = std::move(material_build.material_params_buffer);
    result->material_sampler = std::move(material_build.material_sampler);
    result->textures = std::move(material_build.texture_list);
    return result;
}

} // namespace WhiteBirdEngine
