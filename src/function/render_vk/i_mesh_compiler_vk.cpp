/* Copyright 2026 OppositeNor

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
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/mesh_node_contract.hh"
#include "function/render/mesh_node_contract_resolver.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_resource_list_vk.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/mesh_buffer_compiler_vk.hh"
#include "function/render_vk/mesh_draw_batch_planner_vk.hh"
#include "function/render_vk/mesh_material_resource_builder_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
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

namespace {

/** Name of the reflection-only parameter block that declares the mesh storage-buffer contract. */
constexpr std::string_view MESH_CONTRACT_PARAMETER_NAME = "mesh";

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

Ref<ROBufferVK> create_host_buffer(const BaseRenderDataVK* p_render_data, const void* p_data, size_t p_size, VkBufferUsageFlags p_usage) {
    if (p_size == 0) {
        return MEM_NULL;
    }
    Ref<ROBufferVK> result =
        ROBufferVK::Builder().set_domain(DeviceDomain::HOST_DEVICE).set_size(p_size).set_usage(p_usage).create_ref(p_render_data);
    result->update_buffer(p_data, p_size);
    return result;
}

} // namespace

IMeshCompilerVK::DrawPlan::DrawPlan(RenderAllocator* p_allocator) : commands(p_allocator), batches(p_allocator) {
}

IMeshCompilerVK::IMeshCompilerVK(RendererVK* p_renderer) : renderer(p_renderer) {
    if (renderer == nullptr || renderer->get_base_render_data() == nullptr || renderer->get_base_render_data()->allocator == nullptr) {
        throw std::runtime_error("Failed to construct mesh compiler: renderer state is missing.");
    }
}

const BaseRenderDataVK* IMeshCompilerVK::get_render_data() const {
    return renderer->get_base_render_data();
}

void IMeshCompilerVK::validate_node_layout(const ShaderMetadata& p_vertex_metadata) const {
    MeshNodeContract reflected_contract = MeshNodeContractResolver().resolve(p_vertex_metadata, MESH_CONTRACT_PARAMETER_NAME);
    MeshNodeContract cpu_contract;
    cpu_contract.element_stride = sizeof(GeometryNodeGPU);
    cpu_contract.transform_offset = offsetof(GeometryNodeGPU, transform);
    cpu_contract.normal_transform_offset = offsetof(GeometryNodeGPU, normal_transform);
    if (cpu_contract.matches(reflected_contract)) {
        return;
    }
    throw std::runtime_error(std::format("Mesh node layout mismatch: shader expects stride={}, transform offset={}, normal_transform offset={}, but CPU uploads stride={}, transform offset={}, normal_transform offset={}.",
        reflected_contract.element_stride,
        reflected_contract.transform_offset,
        reflected_contract.normal_transform_offset,
        cpu_contract.element_stride,
        cpu_contract.transform_offset,
        cpu_contract.normal_transform_offset));
}

void IMeshCompilerVK::validate_material(RefRes<RUMaterial> p_material) {
    if (p_material == nullptr) {
        throw std::runtime_error("Failed to compile mesh resources: submesh material is missing.");
    }
}

uint32_t IMeshCompilerVK::get_or_append_material_index(std::vector<RefRes<RUMaterial>>& p_materials, RefRes<RUMaterial> p_material) {
    for (size_t i = 0; i < p_materials.size(); ++i) {
        if (p_materials[i] == p_material) {
            return static_cast<uint32_t>(i);
        }
    }
    if (p_materials.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Failed to compile mesh resources: material table exceeds uint32.");
    }
    p_materials.push_back(p_material);
    return static_cast<uint32_t>(p_materials.size() - 1);
}

Ref<ROBufferVK> IMeshCompilerVK::create_storage_buffer(const void* p_data, size_t p_size) const {
    return create_host_buffer(get_render_data(), p_data, p_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

Ref<ROBufferVK> IMeshCompilerVK::create_indirect_buffer(std::span<const VkDrawIndexedIndirectCommand> p_commands) const {
    return create_host_buffer(get_render_data(),
        p_commands.data(),
        p_commands.size_bytes(),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

MeshBufferCompilerVK::BuildResult IMeshCompilerVK::compile_geometry_buffers(RefRes<RUStaticMesh> p_mesh, const Config& p_config) const {
    return MeshBufferCompilerVK(get_render_data())
        .build(p_mesh,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | p_config.vertex_buffer_usage,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | p_config.index_buffer_usage);
}

MeshBufferCompilerVK::BuildResult IMeshCompilerVK::compile_geometry_buffers(RefRes<RUMesh3D> p_mesh, const Config& p_config) const {
    return MeshBufferCompilerVK(get_render_data())
        .build(p_mesh,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | p_config.vertex_buffer_usage,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | p_config.index_buffer_usage);
}

IMeshCompilerVK::DrawPlan IMeshCompilerVK::plan_draws(
    std::span<const MeshDrawBatchPlannerVK::Draw> p_draws, const ShaderMetadata& p_vertex_metadata, std::string_view p_vertex_entry) const {
    const BaseRenderDataVK* render_data = get_render_data();
    MeshDrawBatchPlannerVK::BuildResult draw_plan = MeshDrawBatchPlannerVK().build(
        render_data->allocator, p_draws, p_vertex_metadata, p_vertex_entry, render_data->max_draw_indirect_count);
    DrawPlan result(render_data->allocator);
    result.commands = std::move(draw_plan.commands);
    result.batches.reserve(draw_plan.batches.size());
    for (MeshDrawBatchPlannerVK::Batch& source_batch : draw_plan.batches) {
        MeshDrawBatchVK draw_batch;
        draw_batch.first_draw = source_batch.first_draw;
        draw_batch.draw_count = source_batch.draw_count;
        draw_batch.vertex_input = std::move(source_batch.vertex_input);
        draw_batch.vertex_buffer_offset = source_batch.vertex_buffer_offset;
        result.batches.push_back(std::move(draw_batch));
    }
    return result;
}

IMeshCompilerVK::MaterialBuild IMeshCompilerVK::compile_materials(const std::optional<MaterialTextureContract>& p_material_contract,
    const Config& p_config,
    std::span<const RefRes<RUMaterial>> p_materials) const {
    const BaseRenderDataVK* render_data = get_render_data();
    MaterialBuild result;
    result.textures = make_ref<RenderResourceListVK>(render_data->allocator, render_data->allocator);
    result.material_sampler = create_material_sampler(render_data, p_material_contract.has_value());
    if (!p_material_contract.has_value()) {
        return result;
    }
    GraphicsPipelineCompilerVKContext compiler_context{
        .allocator = render_data->allocator,
        .render_data = render_data,
        .render_database = renderer->get_render_database(),
        .renderer = renderer,
        .default_texture_resource = get_default_image(p_config.default_material_resource_key),
    };
    GraphicsPipelineCompilerVK texture_compiler(compiler_context);
    std::vector<std::pair<RefRes<RUTexture>, Ref<ROImageViewVK>>> material_texture_views;
    MeshMaterialResourcesVK material_resources = MeshMaterialResourceBuilderVK().build(p_material_contract,
        p_materials,
        get_default_material(p_config.default_material_resource_key),
        render_data->sampled_image_descriptor_capacity,
        result.material_sampler,
        [&texture_compiler, &material_texture_views](RefRes<RUTexture> p_texture) {
            return get_material_texture_view_index(texture_compiler, material_texture_views, p_texture);
        });
    if (material_resources.material_params_data.empty()) {
        throw std::runtime_error("Failed to compile mesh resources: material parameter buffer is missing.");
    }
    result.material_params = create_material_params_buffer(render_data, material_resources.material_params_data);
    for (auto& [texture, image_view] : material_texture_views) {
        static_cast<void>(texture);
        result.textures->add_resource(image_view);
    }
    return result;
}

} // namespace WhiteBirdEngine
