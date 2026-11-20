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
#include "voxel_rend_feature_octree.hh"
#include "../rend_test.hh"
#include "core/alloc/i_alloc.hh"
#include "core/alloc/stl_alloc.hh"
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/rend_vk/compute_pipeline_compiler_vk.hh"
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/mesh_resources_vk.hh"
#include "function/rend_vk/rend_graph_recipe_builder_vk.hh"
#include "function/rend_vk/rend_graph_recipe_params_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/rend_vk/ro_compute_pipeline_vk.hh"
#include "function/rend_vk/ro_image_view_vk.hh"
#include "function/rend_vk/ro_image_vk.hh"
#include "function/rend_vk/rt_run_compute_pipeline_vk.hh"
#include "function/rend_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/rend_vk/rt_triggered_vk.hh"
#include "generated/serializables_sd.gen.hh"
#include "i_voxel_rend_feature.hh"
#include "octree_recipe_params.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "utils/defs.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
namespace {

/**
 * @brief Clears octree textures to OCTREE_EMPTY in every mip level; neither copyable nor movable.
 * @details The first reported writes must be the textures' full views, so the graph moves every mip level into the unified
 * layout before the first clear. The retained images must outlive the task.
 */
class RTOctreeClearVK final : public IRendTaskVK {
public:
    RTOctreeClearVK() = delete;
    RTOctreeClearVK(Vector<Ref<ROImageVK>, RendAlloc> p_images, Vector<std::string, RendAlloc> p_written_resource_ids)
        : images(std::move(p_images)), written_resource_ids(std::move(p_written_resource_ids)) {
    }
    virtual ~RTOctreeClearVK() override = default;
    RTOctreeClearVK(const RTOctreeClearVK&) = delete;
    RTOctreeClearVK(RTOctreeClearVK&&) = delete;
    RTOctreeClearVK& operator=(const RTOctreeClearVK&) = delete;
    RTOctreeClearVK& operator=(RTOctreeClearVK&&) = delete;

    virtual QueueFamilyVK queue_family() const override {
        return QueueFamilyVK::COMPUTE_1;
    }

    virtual void append_accesses(
        Vector<ResAccessVK, RendAlloc>& /*p_reads*/, Vector<ResAccessVK, RendAlloc>& p_writes) const override {
        for (const std::string& resource_id : written_resource_ids) {
            p_writes.push_back({.res_id = resource_id, .usage = RendResUsageVK::TRANSFER_DST});
        }
    }

    virtual void record(VkCommandBuffer p_command_buffer, double /*p_delta*/) override {
        // Mirrors OCTREE_EMPTY of octree/octree_common.slang.
        const VkClearColorValue empty{.uint32 = {255, 255, 255, 255}};
        const VkImageSubresourceRange every_level{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        for (const Ref<ROImageVK>& image : images) {
            vkCmdClearColorImage(p_command_buffer, image->get_image(), VK_IMAGE_LAYOUT_GENERAL, &empty, 1, &every_level);
        }
    }

private:
    Vector<Ref<ROImageVK>, RendAlloc> images;
    Vector<std::string, RendAlloc> written_resource_ids;
};

} // namespace

VoxelRendFeatureOctree::VoxelRendFeatureOctree(
    RendererVK* p_renderer, Ref<MeshResourcesVK> p_static_mesh_resources, const RUAABB3D& p_scene_bounds, bool p_rebuild_static_every_frame)
    : renderer_vk(p_renderer), rend_context(nullptr), rend_alloc(nullptr),
      static_mesh_resources(std::move(p_static_mesh_resources)),
      octree_volume(VoxelVolume::enclose(p_scene_bounds, OCTREE_EXTENT)), rebuild_static_every_frame(p_rebuild_static_every_frame) {
    if (renderer_vk == nullptr) {
        throw std::runtime_error("VoxelRendFeatureOctree: renderer is required.");
    }
    if (static_mesh_resources == MEM_NULL) {
        throw std::runtime_error("VoxelRendFeatureOctree: static scene is required.");
    }
    rend_context = renderer_vk->get_rend_context();
    rend_alloc = rend_context->alloc;
    create_resources();
}

std::string_view VoxelRendFeatureOctree::get_name() const {
    return "octree (mipmapped 3D texture)";
}

const char* VoxelRendFeatureOctree::get_debug_view_name(VoxelDebugViewMode p_mode) const {
    switch (p_mode) {
    case VoxelDebugViewMode::DISABLED:
        return "Disabled";
    case VoxelDebugViewMode::POST_SVO_BUILD:
        return "Octree leaves";
    case VoxelDebugViewMode::PRE_DEFERRED_PROPAGATION:
        return "Octree nodes";
    case VoxelDebugViewMode::OCCUPANCY:
        break;
    }
    throw std::runtime_error("VoxelRendFeatureOctree: unknown voxel debug view mode.");
}

std::string VoxelRendFeatureOctree::get_static_level_resource_id(uint32_t p_level) {
    return std::format("octree_static_levels[{}]", p_level);
}

std::string VoxelRendFeatureOctree::get_dynamic_level_resource_id(uint32_t p_level) {
    return std::format("octree_dynamic_levels[{}]", p_level);
}

Ref<ROImageVK> VoxelRendFeatureOctree::create_octree_image() const {
    return rend_context->create_image(ROImageVK::Builder()
            .image_3d(VkExtent3D{.width = OCTREE_EXTENT, .height = OCTREE_EXTENT, .depth = OCTREE_EXTENT})
            .set_format(OCTREE_FORMAT)
            .set_mipmap_level(OCTREE_LEVEL_COUNT)
            .set_usage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));
}

Ref<ROImageViewVK> VoxelRendFeatureOctree::create_octree_image_view(
    const Ref<ROImageVK>& p_image, uint32_t p_base_level, uint32_t p_level_count) const {
    return rend_context->create_image_view(ROImageViewVK::Builder()
            .set_image(p_image)
            .set_view_type(VK_IMAGE_VIEW_TYPE_3D)
            .set_format(OCTREE_FORMAT)
            .set_subresource_range(VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = p_base_level,
                .levelCount = p_level_count,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }));
}

Ref<ROBufferVK> VoxelRendFeatureOctree::create_initialized_buffer(VkBufferUsageFlags p_usage, const void* p_data, size_t p_size) const {
    return rend_context->create_buffer(ROBufferVK::Builder().set_usage(p_usage).init_data(p_data, p_size));
}

void VoxelRendFeatureOctree::create_resources() {
    static_image = create_octree_image();
    dynamic_image = create_octree_image();
    static_view = create_octree_image_view(static_image, 0, OCTREE_LEVEL_COUNT);
    dynamic_view = create_octree_image_view(dynamic_image, 0, OCTREE_LEVEL_COUNT);
    for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
        static_level_views[level] = create_octree_image_view(static_image, level, 1);
        dynamic_level_views[level] = create_octree_image_view(dynamic_image, level, 1);
    }
    const OctreeInfo info{
        .origin = octree_volume.origin,
        .width = octree_volume.width,
        .max_depth = OCTREE_MAX_DEPTH,
        .max_debug_node_count = OCTREE_MAX_DEBUG_NODE_COUNT,
        .padding = {},
    };
    octree_info = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &info, sizeof(info));
    debug_draw_index_buffer = create_initialized_buffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VOXEL_DEBUG_CUBE_INDICES.data(), sizeof(VOXEL_DEBUG_CUBE_INDICES));
    const VkDrawIndexedIndirectCommand init_debug_draw_command{
        .indexCount = VOXEL_DEBUG_CUBE_INDICES.size(),
        .instanceCount = 0,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    };
    // The leaf gather writes every command before each leaf-view draw.
    debug_draw_indirect_buffer = rend_context->create_buffer(ROBufferVK::Builder().set_properties(sizeof(VkDrawIndexedIndirectCommand),
        OCTREE_DEBUG_LEAF_DRAW_COUNT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT));
    debug_leaf_masks = rend_context->create_buffer(ROBufferVK::Builder().set_properties(
        OCTREE_DEBUG_LEAF_MASK_SIZE, OCTREE_DEBUG_SUBGRID_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    debug_node_draw_indirect_buffer = create_initialized_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        &init_debug_draw_command,
        sizeof(init_debug_draw_command));
    debug_node_list = rend_context->create_buffer(ROBufferVK::Builder().set_properties(
        sizeof(uint32_t), size_t{OCTREE_DEBUG_NODE_LIST_HEADER_COUNT} + OCTREE_MAX_DEBUG_NODE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
}

RendTaskDescVK VoxelRendFeatureOctree::get_clear_rend_task_desc(std::string_view p_task_id, bool p_clear_static) const {
    RendTaskDescVK task_desc(rend_alloc);
    task_desc.task_id = p_task_id;
    task_desc.factory = [this, p_clear_static](const RendGraphContextVK& /*p_context*/) {
        Vector<Ref<ROImageVK>, RendAlloc> images(rend_alloc);
        Vector<std::string, RendAlloc> written_resource_ids(rend_alloc);
        // The full views come first, so the graph's first-use layout transition covers every mip level.
        if (p_clear_static) {
            images.push_back(static_image);
            written_resource_ids.emplace_back(OCTREE_STATIC);
        }
        images.push_back(dynamic_image);
        written_resource_ids.emplace_back(OCTREE_DYNAMIC);
        for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
            if (p_clear_static) {
                written_resource_ids.push_back(get_static_level_resource_id(level));
            }
            written_resource_ids.push_back(get_dynamic_level_resource_id(level));
        }
        return make_unique<RTOctreeClearVK>(rend_alloc, std::move(images), std::move(written_resource_ids));
    };
    return task_desc;
}

RendTaskDescVK VoxelRendFeatureOctree::make_static_build(RendTaskDescVK p_task, const VoxelRecipeInputs& p_inputs) const {
    if (rebuild_static_every_frame) {
        // The reset stage empties both textures, so every frame rebuilds from an empty octree.
        return p_task;
    }
    p_task.factory = RTTriggeredVK::wrap_factory(
        std::move(p_task.factory), std::string(p_inputs.static_build_event), rend_context, p_inputs.static_build_pending);
    return p_task;
}

void VoxelRendFeatureOctree::configure_recipe(
    DataValue& p_parameters, RendGraphRecipeImportsVK& p_imports, const VoxelRecipeInputs& p_inputs) const {
    OctreeRecipeParams params;
    params.module = "rend_graph_utils.features.voxel_octree";
    params.level_count = OCTREE_LEVEL_COUNT;
    params.leaf_gather_group_count = OctreeGroupCount{
        .x = OCTREE_DEBUG_SUBGRID_GRID_EXTENT.width,
        .y = OCTREE_DEBUG_SUBGRID_GRID_EXTENT.height,
        .z = OCTREE_DEBUG_SUBGRID_GRID_EXTENT.depth,
    };
    params.node_gather_group_count = OCTREE_DEBUG_GATHER_GROUP_COUNT;
    params.leaf_draw_count = OCTREE_DEBUG_LEAF_DRAW_COUNT;
    params.extent = OCTREE_EXTENT;
    if (!rebuild_static_every_frame) {
        params.static_build =
            RendRecipeTriggerVK{.event = std::string(p_inputs.static_build_event), .activate = p_inputs.static_build_pending};
    }

    // Textures and buffers.
    const auto import = [&p_imports](std::string_view p_name, const IBindableRendResourceVK& p_resource) {
        p_imports.add(p_name, p_resource);
    };
    import(OCTREE_INFO, *octree_info);
    import(OCTREE_STATIC, *static_view);
    import(OCTREE_DYNAMIC, *dynamic_view);
    import(OCTREE_DEBUG_DRAW_INDEX_BUFFER, *debug_draw_index_buffer);
    import(OCTREE_DEBUG_DRAW_INDIRECT_BUFFER, *debug_draw_indirect_buffer);
    import(OCTREE_DEBUG_LEAF_MASKS, *debug_leaf_masks);
    import(OCTREE_DEBUG_NODE_DRAW_INDIRECT_BUFFER, *debug_node_draw_indirect_buffer);
    import(OCTREE_DEBUG_NODE_LIST, *debug_node_list);
    for (uint32_t level = 0; level < OCTREE_LEVEL_COUNT; ++level) {
        import(get_static_level_resource_id(level), *static_level_views[level]);
        import(get_dynamic_level_resource_id(level), *dynamic_level_views[level]);
    }
    SerializableSD<OctreeRecipeParams>::serialize(p_parameters, params);

    // The clears record vkCmdClearColorImage, so they are C++ tasks the recipe imports; the static one records once. The
    // module draws the construction passes, whose shaders report the full texture view they write through the per-level
    // views.
    p_imports.add_task(make_static_build(get_clear_rend_task_desc(RT_RESET_OCTREE, true), p_inputs))
        .add_task(get_clear_rend_task_desc(RT_CLEAR_DYNAMIC_OCTREE, false));
}

} // namespace WhiteBirdEngine
