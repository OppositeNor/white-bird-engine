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
#include "function/render_vk/render_graph_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/descriptor_heap_allocator_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/roc_multislots_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "function/render_vk/semaphore_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

void register_resource_list(Map<std::string, Ref<RendResVK>, RenderAllocator>* p_resources,
    RenderAllocator* p_allocator,
    std::string_view p_resource_id,
    Ref<RenderResourceList> p_resource_list) {
    if (p_resources == nullptr || p_allocator == nullptr || p_resource_id.empty() || p_resource_list == MEM_NULL) {
        throw std::runtime_error("RenderGraphVK: invalid render resource list registration.");
    }
    if (p_resources->contains(p_resource_id)) {
        throw std::runtime_error(std::format("RenderGraphVK: duplicate resource id: {}.", p_resource_id));
    }
    Ref<RendResVK> aggregate = make_ref<RendResVK>(p_allocator);
    aggregate->res_id = p_resource_id;
    aggregate->res_type_id = static_hash("RenderResourceList");
    aggregate->resource_list = p_resource_list;
    p_resources->emplace(std::string(p_resource_id), std::move(aggregate));

    for (size_t index = 0; index < p_resource_list->get_resource_count(); ++index) {
        const std::string indexed_id = std::format("{}[{}]", p_resource_id, index);
        if (p_resources->contains(indexed_id)) {
            throw std::runtime_error(std::format("RenderGraphVK: duplicate resource-list entry id: {}.", indexed_id));
        }
        Ref<IRenderObjectVK> resource = p_resource_list->acquire_resource(index).dynamic_cast_ref<IRenderObjectVK>();
        if (resource == MEM_NULL) {
            throw std::runtime_error(std::format("RenderGraphVK: resource-list entry '{}' is not a Vulkan render object.", indexed_id));
        }
        Ref<ROCSingleVK> container = make_ref<ROCSingleVK>(p_allocator, std::move(resource));
        Ref<RendResVK> indexed_resource = make_ref<RendResVK>(p_allocator);
        indexed_resource->res_id = indexed_id;
        indexed_resource->res_type_id = container->get_resource_type_id();
        indexed_resource->container = std::move(container);
        p_resources->emplace(indexed_id, std::move(indexed_resource));
    }
}

Ref<IRenderObjectVK> resolve_render_object(const RendResVK& p_resource, uint32_t p_slot) {
    if (p_resource.container == MEM_NULL) {
        return MEM_NULL;
    }
    return p_resource.container->get(p_slot).dynamic_cast_ref<IRenderObjectVK>();
}

DebugData make_descriptor_data(const DebugDataRows& p_rows) {
    DebugDataArray rows;
    rows.reserve(p_rows.size());
    for (const DebugData& row : p_rows) {
        rows.push_back(row);
    }
    DebugData root;
    root.set(std::move(rows));
    return root;
}

uint32_t descriptor_heap_index(const DebugData& p_row) {
    return p_row.get_value<uint32_t>("heap_index");
}

std::string descriptor_heap(const DebugData& p_row) {
    return p_row.get_value<std::string>("heap");
}

std::string descriptor_resource_id(const DebugData& p_row) {
    return p_row.get_value<std::string>("resource_id");
}

int64_t descriptor_slot_index(const DebugData& p_row) {
    if (!p_row.contains("slot_index")) {
        return -1;
    }
    return static_cast<int64_t>(p_row.get_value<uint32_t>("slot_index"));
}

std::string descriptor_type_name(const DebugData& p_row) {
    return p_row.get_value<std::string>("descriptor_type");
}

VkQueue queue_for(const BaseRenderDataVK* p_render_data, QueueFamilyVK p_queue_family) {
    switch (p_queue_family) {
    case QueueFamilyVK::GRAPHICS:
        return p_render_data->graphics_queue;
    case QueueFamilyVK::COMPUTE:
        return p_render_data->compute_queue;
    }
    return VK_NULL_HANDLE;
}

VkCommandPool pool_for(const BaseRenderDataVK* p_render_data, QueueFamilyVK p_queue_family) {
    switch (p_queue_family) {
    case QueueFamilyVK::GRAPHICS:
        return p_render_data->graphics_command_pool;
    case QueueFamilyVK::COMPUTE:
        return p_render_data->compute_command_pool;
    }
    return VK_NULL_HANDLE;
}

struct ResourceScopeVK {
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
};

QueueFamilyVK node_queue_family(Ref<RGNode> p_node) {
    return p_node->in_queue;
}

VkPipelineStageFlags2 shader_stage_scope(QueueFamilyVK p_queue_family) {
    switch (p_queue_family) {
    case QueueFamilyVK::GRAPHICS:
        return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    case QueueFamilyVK::COMPUTE:
        return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }
    throw std::runtime_error("RenderGraphVK: unknown queue family.");
}

void ensure_graphics_queue_usage(QueueFamilyVK p_queue_family, RendResUsageVK p_usage) {
    if (p_queue_family == QueueFamilyVK::COMPUTE) {
        throw std::runtime_error(
            std::format("RenderGraphVK: resource usage {} is not valid on the compute queue.", std::to_underlying(p_usage)));
    }
}

ResourceScopeVK resource_scope(RendResUsageVK p_usage, bool p_write, QueueFamilyVK p_queue_family) {
    switch (p_usage) {
    case RendResUsageVK::SAMPLED_IMAGE:
        if (p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::COLOR_ATTACHMENT:
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = p_write ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    case RendResUsageVK::DEPTH_ATTACHMENT:
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = p_write ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT};
    case RendResUsageVK::STORAGE_IMAGE:
        return {.access = p_write ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::UNIFORM_BUFFER:
        if (p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_UNIFORM_READ_BIT, .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::STORAGE_BUFFER:
        return {.access = p_write ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            .stage = shader_stage_scope(p_queue_family)};
    case RendResUsageVK::ACCELERATION_STRUCTURE:
        return {.access = p_write ? VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR : VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .stage = p_write ? VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR : shader_stage_scope(p_queue_family)};
    case RendResUsageVK::TRANSFER_SRC:
        if (p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_TRANSFER_READ_BIT, .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    case RendResUsageVK::TRANSFER_DST:
        if (!p_write) {
            break;
        }
        return {.access = VK_ACCESS_2_TRANSFER_WRITE_BIT, .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT};
    case RendResUsageVK::INDIRECT_BUFFER:
        if (p_write) {
            break;
        }
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, .stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT};
    case RendResUsageVK::VERTEX_BUFFER:
        if (p_write) {
            break;
        }
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, .stage = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT};
    case RendResUsageVK::INDEX_BUFFER:
        if (p_write) {
            break;
        }
        ensure_graphics_queue_usage(p_queue_family, p_usage);
        return {.access = VK_ACCESS_2_INDEX_READ_BIT, .stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT};
    }
    throw std::runtime_error("RenderGraphVK: resource usage is invalid for the declared read/write direction.");
}

void validate_declared_resource(
    const Map<std::string, Ref<RendResVK>, RenderAllocator>& p_resources, const RGNode& p_node, const ResAccessVK& p_access, bool p_is_write) {
    if (!p_resources.contains(p_access.res_id)) {
        throw std::runtime_error(std::format("RenderGraphVK: task '{}' {} undeclared resource '{}'.",
            p_node.task_id,
            p_is_write ? "writes" : "reads",
            p_access.res_id));
    }
}

// Stages that only exist on graphics-capable queues.
constexpr VkPipelineStageFlags2 GRAPHICS_ONLY_STAGES =
    VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
    VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
    VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
    VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT |
    VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT |
    VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

// Expand meta stages so that stage-set intersections can be computed bitwise.
VkPipelineStageFlags2 expand_stages(VkPipelineStageFlags2 p_stages) {
    if ((p_stages & VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) != 0) {
        return ~VkPipelineStageFlags2{0};
    }
    if ((p_stages & VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) != 0) {
        p_stages |= GRAPHICS_ONLY_STAGES | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    }
    if ((p_stages & VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT) != 0) {
        p_stages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
                    VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT |
                    VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
    }
    if ((p_stages & VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT) != 0) {
        p_stages |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    }
    if ((p_stages & VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT) != 0) {
        p_stages |= VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT |
                    VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;
    }
    return p_stages;
}

void validate_edge_stage(const RGNode& p_node, VkPipelineStageFlags2 p_stage, const RGResEdge& p_edge, std::string_view p_side) {
    if (p_stage == VK_PIPELINE_STAGE_2_NONE) {
        throw std::runtime_error(std::format("RenderGraphVK: edge '{}' -> '{}' on resource '{}' has an empty {} stage.",
            p_edge.from->task_id,
            p_edge.to->task_id,
            p_edge.res_id,
            p_side));
    }
    if (p_node.in_queue == QueueFamilyVK::COMPUTE && (p_stage & GRAPHICS_ONLY_STAGES) != 0) {
        throw std::runtime_error(std::format("RenderGraphVK: edge '{}' -> '{}' on resource '{}' uses graphics-only {} stages on compute task '{}'.",
            p_edge.from->task_id,
            p_edge.to->task_id,
            p_edge.res_id,
            p_side,
            p_node.task_id));
    }
}

bool node_accesses(const Vector<ResAccessVK, RenderAllocator>& p_accesses, std::string_view p_resource_id) {
    return std::ranges::any_of(p_accesses, [p_resource_id](const ResAccessVK& p_access) {
        return p_access.res_id == p_resource_id;
    });
}

VkAccessFlags2 edge_dst_access(RendEdgeTypeVK p_type) {
    switch (p_type) {
    case RendEdgeTypeVK::READ:
        return VK_ACCESS_2_MEMORY_READ_BIT;
    case RendEdgeTypeVK::WRITE:
        return VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    }
    throw std::runtime_error("RenderGraphVK: unknown resource edge type.");
}

VkBufferUsageFlags buffer_usage_flags(RendResPrimaryUsageVK p_usage, std::string_view p_resource_id) {
    switch (p_usage) {
    case RendResPrimaryUsageVK::BUFFER_UNIFORM:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case RendResPrimaryUsageVK::BUFFER_STORAGE:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case RendResPrimaryUsageVK::BUFFER_INDIRECT:
        return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case RendResPrimaryUsageVK::BUFFER_VERTEX:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case RendResPrimaryUsageVK::BUFFER_INDEX:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    default:
        break;
    }
    throw std::runtime_error(std::format("RenderGraphVK: buffer '{}' has a non-buffer primary usage.", p_resource_id));
}

VkImageUsageFlags image_usage_flags(RendResPrimaryUsageVK p_usage, std::string_view p_resource_id) {
    switch (p_usage) {
    case RendResPrimaryUsageVK::IMAGE_READ_ONLY:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    case RendResPrimaryUsageVK::IMAGE_READ_WRITE:
        return VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    case RendResPrimaryUsageVK::IMAGE_RENDER_TARGET:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    case RendResPrimaryUsageVK::IMAGE_DEPTH_TARGET:
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    default:
        break;
    }
    throw std::runtime_error(std::format("RenderGraphVK: image '{}' has a non-image primary usage.", p_resource_id));
}

ROSamplerVK::Builder make_sampler_builder(const RendSamplerDescVK& p_desc) {
    ROSamplerVK::Builder builder;
    builder.set_filter(p_desc.min_filter, p_desc.mag_filter)
        .set_mipmap_mode(p_desc.mipmap_mode)
        .set_address_mode(p_desc.address_mode_u, p_desc.address_mode_v, p_desc.address_mode_w)
        .set_mip_lod_bias(p_desc.mip_lod_bias)
        .set_anisotropy(p_desc.max_anisotropy.has_value() ? VK_TRUE : VK_FALSE, p_desc.max_anisotropy.value_or(1.0F))
        .set_compare(p_desc.compare_op.has_value() ? VK_TRUE : VK_FALSE, p_desc.compare_op.value_or(VK_COMPARE_OP_ALWAYS))
        .set_lod_range(p_desc.min_lod, p_desc.max_lod)
        .set_border_color(p_desc.border_color)
        .set_unnormalized_coordinates(p_desc.unnormalized_coordinates ? VK_TRUE : VK_FALSE);
    return builder;
}

bool is_depth_format(VkFormat p_format) {
    switch (p_format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

Ref<ROImageViewVK> make_image_view(const BaseRenderDataVK* p_render_data, Ref<ROImageVK> p_image, const RendImageViewDeclVK& p_view_decl) {
    const RendImageViewDescVK& desc = p_view_decl.desc;
    const ROImageVK::ExposedFields& image = p_image->fields();
    if (desc.base_mip_level >= image.mipmap_levels || desc.base_array_layer >= image.layers_count) {
        throw std::runtime_error(
            std::format("RenderGraphVK: image view '{}' starts outside image '{}'.", p_view_decl.res_id, p_view_decl.image_id));
    }
    const uint32_t mip_level_count =
        desc.mip_level_count == VK_REMAINING_MIP_LEVELS ? image.mipmap_levels - desc.base_mip_level : desc.mip_level_count;
    const uint32_t layer_count =
        desc.layer_count == VK_REMAINING_ARRAY_LAYERS ? image.layers_count - desc.base_array_layer : desc.layer_count;
    if (mip_level_count == 0 || layer_count == 0 || mip_level_count > image.mipmap_levels - desc.base_mip_level ||
        layer_count > image.layers_count - desc.base_array_layer) {
        throw std::runtime_error(
            std::format("RenderGraphVK: image view '{}' exceeds image '{}'.", p_view_decl.res_id, p_view_decl.image_id));
    }
    const VkFormat format = desc.format.value_or(image.format);
    return ROImageViewVK::Builder()
        .set_image(p_image)
        .set_view_type(desc.view_type.value_or(image.view_type))
        .set_format(format)
        .set_subresource_range(VkImageSubresourceRange{
            .aspectMask = desc.aspect_mask.value_or(is_depth_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            .baseMipLevel = desc.base_mip_level,
            .levelCount = mip_level_count,
            .baseArrayLayer = desc.base_array_layer,
            .layerCount = layer_count,
        })
        .create_ref(p_render_data);
}

} // namespace

RenderGraphVK::RenderGraphVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data), nodes(p_render_data->allocator), resource_edges(p_render_data->allocator),
      all_resources(p_render_data->allocator), command_buffers(p_render_data->allocator),
      command_buffer_pools(p_render_data->allocator), terminal_semaphores(p_render_data->allocator) {
}

RenderGraphVK::~RenderGraphVK() {
    free_command_buffers();
}

std::string_view RendResViewVK::resolve(std::string_view p_resource_id) const {
    return resource_map != nullptr ? resource_map->resolve(p_resource_id) : p_resource_id;
}

bool RendResViewVK::contains(std::string_view p_resource_id) const {
    auto resource = resources->find(resolve(p_resource_id));
    return resource != resources->end() && resource->second != MEM_NULL &&
           (resource->second->container != MEM_NULL || resource->second->resource_list != MEM_NULL);
}

const RendResVK& RendResViewVK::get(std::string_view p_resource_id) const {
    const std::string_view graph_resource_id = resolve(p_resource_id);
    auto resource = resources->find(graph_resource_id);
    if (resource == resources->end() || resource->second == MEM_NULL ||
        (resource->second->container == MEM_NULL && resource->second->resource_list == MEM_NULL)) {
        if (graph_resource_id != p_resource_id) {
            throw std::runtime_error(
                std::format("Render graph resource was not declared: {} (mapped from {}).", graph_resource_id, p_resource_id));
        }
        throw std::runtime_error(std::format("Render graph resource was not declared: {}.", p_resource_id));
    }
    return *resource->second;
}

DebugDataRows RendResViewVK::descriptor_debug_data() const {
    RenderAllocator* allocator = resources->get_allocator().allocator;
    DebugDataRows rows(allocator);
    for (const auto& [resource_id, resource] : *resources) {
        if (resource == MEM_NULL || resource->container == MEM_NULL) {
            continue;
        }
        DebugDataRows resource_rows = resource->container->descriptor_debug_data();
        for (DebugData& row : resource_rows) {
            row.set_value("resource_id", resource_id);
            rows.emplace_back(std::move(row));
        }
    }
    std::sort(rows.begin(), rows.end(), [](const DebugData& p_left, const DebugData& p_right) {
        return std::make_tuple(descriptor_heap_index(p_left),
                   descriptor_heap(p_left),
                   descriptor_resource_id(p_left),
                   descriptor_slot_index(p_left),
                   descriptor_type_name(p_left)) < std::make_tuple(descriptor_heap_index(p_right),
                                                       descriptor_heap(p_right),
                                                       descriptor_resource_id(p_right),
                                                       descriptor_slot_index(p_right),
                                                       descriptor_type_name(p_right));
    });
    return rows;
}

Ref<RGNode> RenderGraphVK::add_node(Unique<IRenderTaskVK> p_task) {
    auto node = make_ref<RGNode>(render_data->allocator, render_data->allocator);
    node->render_task = std::move(p_task);
    node->in_queue = node->render_task->queue_family();
    nodes.emplace_back(node);
    return node;
}

Ref<RGNode> RenderGraphVK::find_node(std::string_view p_task_id) const {
    for (Ref<RGNode> node : nodes) {
        if (node->task_id == p_task_id) {
            return node;
        }
    }
    return MEM_NULL;
}

void RenderGraphVK::add_resource_edge(
    Ref<RGNode> p_from, Ref<RGNode> p_to, std::string p_resource_id, RendEdgeTypeVK p_type, VkPipelineStageFlags2 p_src_stage, VkPipelineStageFlags2 p_dst_stage) {
    auto edge = make_ref<RGResEdge>(render_data->allocator);
    edge->from = p_from;
    edge->to = p_to;
    edge->res_id = std::move(p_resource_id);
    edge->type = p_type;
    edge->src_stage = p_src_stage;
    edge->dst_stage = p_dst_stage;
    p_from->out_edges.emplace_back(edge);
    p_to->in_edges.emplace_back(edge);
    resource_edges.emplace_back(std::move(edge));
}

void RenderGraphVK::topological_sort() { // NOLINT
    // Kahn's algorithm over resource edges. Parallel edges between the same pair of nodes each count once.
    Vector<size_t, RenderAllocator> in_degree(nodes.size(), 0, render_data->allocator);
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i]->topo_order = i;
    }
    for (const Ref<RGResEdge>& edge : resource_edges) {
        ++in_degree[edge->to->topo_order];
    }
    Vector<Ref<RGNode>, RenderAllocator> sorted(render_data->allocator);
    sorted.reserve(nodes.size());
    Vector<size_t, RenderAllocator> ready(render_data->allocator);
    // Seed in reverse so that independent tasks keep their descriptor order.
    for (size_t i = nodes.size(); i > 0; --i) {
        if (in_degree[i - 1] == 0) {
            ready.push_back(i - 1);
        }
    }
    while (!ready.empty()) {
        size_t node_index = ready.back();
        ready.pop_back();
        Ref<RGNode> node = nodes[node_index];
        sorted.push_back(node);
        for (auto& weak_edge : node->out_edges) {
            Ref<RGResEdge> edge = weak_edge.lock();
            WBE_DEBUG_ASSERT(edge != MEM_NULL);
            size_t consumer_index = edge->to->topo_order;
            --in_degree[consumer_index];
            if (in_degree[consumer_index] == 0) {
                ready.push_back(consumer_index);
            }
        }
    }
    if (sorted.size() != nodes.size()) {
        std::string cyclic_tasks;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (in_degree[i] > 0) {
                cyclic_tasks += std::format("{}'{}'", cyclic_tasks.empty() ? "" : ", ", nodes[i]->task_id);
            }
        }
        throw std::runtime_error(std::format("RenderGraphVK: render graph contains a cycle; unresolved tasks: {}.", cyclic_tasks));
    }
    nodes = std::move(sorted);
    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i]->topo_order = i;
    }
}

void RenderGraphVK::allocate_command_buffers() {
    const uint32_t frames = std::max<uint32_t>(render_data->frames_in_flight, 1U);
    const size_t total = static_cast<size_t>(frames) * nodes.size();
    command_buffers.assign(total, VK_NULL_HANDLE);
    command_buffer_pools.assign(total, VK_NULL_HANDLE);
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto queue_family = nodes[i]->in_queue;
        VkCommandPool command_pool = pool_for(render_data, queue_family);
        if (command_pool == VK_NULL_HANDLE) {
            throw std::runtime_error(
                std::format("Failed to create command pool for queue family: {}.", std::to_underlying(queue_family)));
        }
        for (uint32_t f = 0; f < frames; ++f) {
            const size_t slot = static_cast<size_t>(f) * nodes.size() + i;
            command_buffer_pools[slot] = command_pool;
            VkCommandBufferAllocateInfo alloc_info{};
            alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            alloc_info.commandPool = command_pool;
            alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            alloc_info.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(render_data->logic_device, &alloc_info, &command_buffers[slot]) != VK_SUCCESS) {
                throw std::runtime_error("RenderGraphVK: failed to allocate command buffer.");
            }
        }
    }
}

void RenderGraphVK::free_command_buffers() {
    const bool has_allocated_command_buffer = std::ranges::any_of(command_buffers, [](VkCommandBuffer p_command_buffer) {
        return p_command_buffer != VK_NULL_HANDLE;
    });
    if (has_allocated_command_buffer) {
        vkDeviceWaitIdle(render_data->logic_device);
    }
    for (size_t i = 0; i < command_buffers.size(); ++i) {
        if (command_buffers[i] != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(render_data->logic_device, command_buffer_pools[i], 1, &command_buffers[i]);
            command_buffers[i] = VK_NULL_HANDLE;
        }
    }
    command_buffers.clear();
    command_buffer_pools.clear();
}

uint32_t RenderGraphVK::terminal_semaphore_index() const {
    if (terminal_semaphores.empty()) {
        throw std::runtime_error("RenderGraphVK: terminal semaphores are not allocated.");
    }
    if (render_data->current_swap_chain_index >= terminal_semaphores.size()) {
        throw std::runtime_error("RenderGraphVK: swap-chain image index exceeds terminal semaphore count.");
    }
    return render_data->current_swap_chain_index;
}

Ref<SemaphoreVK> RenderGraphVK::get_terminal_semaphore() const {
    return terminal_semaphores[terminal_semaphore_index()];
}

DebugData RenderGraphVK::list_descriptors() const {
    return make_descriptor_data(RendResViewVK(&all_resources).descriptor_debug_data());
}

void RenderGraphVK::build_from_descriptor(const RendGraphDescVK& p_desc) {
    free_command_buffers();
    nodes.clear();
    resource_edges.clear();
    all_resources.clear();

    all_resources = index_resources(p_desc);
    allocate_resources(p_desc);
    instantiate_tasks(p_desc);
    add_resource_edges(p_desc);

    topological_sort();
    validate_resource_edges();
    validate_resource_conflicts();
    validate_queue_completion();
    build_synchronization();

    allocate_command_buffers();

    // Allocate one terminal binary semaphore per swap-chain image. Presentation
    // may retain the wait semaphore until that exact image is acquired again.
    const uint32_t swap_chain_images = std::max<uint32_t>(render_data->swap_chain_images_count, 1U);
    terminal_semaphores.clear();
    terminal_semaphores.reserve(swap_chain_images);
    for (uint32_t i = 0; i < swap_chain_images; ++i) {
        terminal_semaphores.emplace_back(make_ref<SemaphoreVK>(render_data->allocator, render_data, true));
    }
}

Map<std::string, Ref<RendResVK>, RenderAllocator> RenderGraphVK::index_resources(const RendGraphDescVK& p_desc) {
    Map<std::string, Ref<RendResVK>, RenderAllocator> resources_by_id(render_data->allocator);
    resources_by_id.reserve(p_desc.resources.size());
    for (const auto& resource_desc : p_desc.resources) {
        if (resource_desc.res_id.empty()) {
            throw std::runtime_error("RenderGraphVK: resource id is empty.");
        }
        if (resource_desc.resource_list != MEM_NULL) {
            if (resource_desc.container != MEM_NULL) {
                throw std::runtime_error(
                    std::format("RenderGraphVK: aggregate resource '{}' also has a concrete container.", resource_desc.res_id));
            }
            register_resource_list(&resources_by_id, render_data->allocator, resource_desc.res_id, resource_desc.resource_list);
            continue;
        }
        if (resource_desc.container == MEM_NULL) {
            throw std::runtime_error(
                std::format("RenderGraphVK: resource '{}' must be bound before graph build.", resource_desc.res_id));
        }
        if (resource_desc.container->get_resource_type_id() != resource_desc.res_type_id) {
            throw std::runtime_error(
                std::format("RenderGraphVK: resource container type does not match declaration for '{}'.", resource_desc.res_id));
        }
        if (resources_by_id.contains(resource_desc.res_id)) {
            throw std::runtime_error(std::format("RenderGraphVK: duplicate resource id: {}.", resource_desc.res_id));
        }
        auto resource_ref = make_ref<RendResVK>(render_data->allocator);
        resource_ref->res_id = resource_desc.res_id;
        resource_ref->res_type_id = resource_desc.res_type_id;
        resource_ref->container = resource_desc.container;
        resources_by_id.emplace(resource_desc.res_id, std::move(resource_ref));
    }
    return resources_by_id;
}

void RenderGraphVK::register_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_container) {
    if (all_resources.contains(p_resource_id)) {
        throw std::runtime_error(std::format("RenderGraphVK: duplicate resource id: {}.", p_resource_id));
    }
    auto resource = make_ref<RendResVK>(render_data->allocator);
    resource->res_id = p_resource_id;
    resource->res_type_id = p_container->get_resource_type_id();
    resource->container = std::move(p_container);
    all_resources.emplace(std::string(p_resource_id), std::move(resource));
}

Ref<RenderObjectContainerVK> RenderGraphVK::create_container(
    bool p_per_frame, const std::function<Ref<IRenderObjectVK>(uint32_t)>& p_factory) const {
    if (p_per_frame) {
        const uint32_t frames = std::max<uint32_t>(render_data->frames_in_flight, 1U);
        return make_ref<ROCMultislotsVK>(render_data->allocator, render_data->allocator, frames, p_factory);
    }
    return make_ref<ROCSingleVK>(render_data->allocator, p_factory(0));
}

void RenderGraphVK::allocate_resources(const RendGraphDescVK& p_desc) {
    // Reject duplicate ids before any GPU object is created.
    Set<std::string_view, RenderAllocator> declared_ids(render_data->allocator);
    auto declare = [this, &declared_ids](std::string_view p_resource_id) {
        if (all_resources.contains(p_resource_id) || !declared_ids.insert(p_resource_id).second) {
            throw std::runtime_error(std::format("RenderGraphVK: duplicate resource id: {}.", p_resource_id));
        }
    };
    for (const RendBufferDeclVK& buffer_decl : p_desc.buffers) {
        declare(buffer_decl.res_id);
    }
    for (const RendImageDeclVK& image_decl : p_desc.images) {
        declare(image_decl.res_id);
    }
    for (const RendImageViewDeclVK& view_decl : p_desc.image_views) {
        declare(view_decl.res_id);
    }
    for (const RendSamplerDeclVK& sampler_decl : p_desc.samplers) {
        declare(sampler_decl.res_id);
    }

    for (const RendBufferDeclVK& buffer_decl : p_desc.buffers) {
        const RendBufferDescVK& desc = buffer_decl.desc;
        const size_t size = desc.size == 0 ? desc.initial_data.size() : desc.size;
        if (size == 0) {
            throw std::runtime_error(std::format("RenderGraphVK: buffer '{}' has no size.", buffer_decl.res_id));
        }
        if (desc.initial_data.size() > size) {
            throw std::runtime_error(std::format("RenderGraphVK: initial data of buffer '{}' exceeds its size.", buffer_decl.res_id));
        }
        ROBufferVK::Builder builder;
        if (!desc.initial_data.empty()) {
            builder.initial_data(desc.initial_data, desc.domain);
        }
        builder.set_size(size).set_domain(desc.domain).add_usage(buffer_usage_flags(desc.usage, buffer_decl.res_id) | desc.additional_usage);
        register_resource(buffer_decl.res_id, create_container(desc.per_frame, [this, &builder](uint32_t /*p_slot*/) -> Ref<IRenderObjectVK> {
            return builder.create_ref(render_data);
        }));
    }
    for (const RendImageDeclVK& image_decl : p_desc.images) {
        const RendImageDescVK& desc = image_decl.desc;
        if (desc.format == VK_FORMAT_UNDEFINED || desc.extent.width == 0 || desc.extent.height == 0 ||
            desc.extent.depth == 0 || desc.mip_levels == 0 || desc.layers == 0) {
            throw std::runtime_error(std::format(
                "RenderGraphVK: image '{}' has an undefined format, extent, mip count, or layer count.", image_decl.res_id));
        }
        ROImageVK::Builder builder;
        builder.set_image_type(desc.image_type)
            .set_format(desc.format)
            .set_extent(desc.extent)
            .set_mipmap_level(desc.mip_levels)
            .set_layers(desc.layers)
            .set_usage(image_usage_flags(desc.usage, image_decl.res_id) | desc.additional_usage);
        register_resource(image_decl.res_id, create_container(desc.per_frame, [this, &builder](uint32_t /*p_slot*/) -> Ref<IRenderObjectVK> {
            return builder.create_ref(render_data);
        }));
    }
    for (const RendImageViewDeclVK& view_decl : p_desc.image_views) {
        auto image_resource = all_resources.find(view_decl.image_id);
        if (image_resource == all_resources.end() || image_resource->second->container == MEM_NULL ||
            image_resource->second->res_type_id != TypeUUID<ROImageVK>::UUID) {
            throw std::runtime_error(std::format("RenderGraphVK: image view '{}' views '{}', which is not a declared image resource.",
                view_decl.res_id,
                view_decl.image_id));
        }
        Ref<RenderObjectContainerVK> image_container = image_resource->second->container;
        const bool per_frame = image_container.dynamic_cast_ref<ROCMultislotsVK>() != MEM_NULL;
        auto create_view = [this, &view_decl, &image_container](uint32_t p_slot) -> Ref<IRenderObjectVK> {
            Ref<ROImageVK> image = image_container->get(p_slot).dynamic_cast_ref<ROImageVK>();
            WBE_DEBUG_ASSERT(image != MEM_NULL);
            return make_image_view(render_data, image, view_decl);
        };
        register_resource(view_decl.res_id, create_container(per_frame, create_view));
    }
    for (const RendSamplerDeclVK& sampler_decl : p_desc.samplers) {
        register_resource(sampler_decl.res_id, create_container(false, [this, &sampler_decl](uint32_t /*p_slot*/) -> Ref<IRenderObjectVK> {
            return make_sampler_builder(sampler_decl.desc).create_ref(render_data);
        }));
    }
}

Ref<RenderObjectContainerVK> RenderGraphVK::acquire_resource(std::string_view p_resource_id) const {
    auto resource = all_resources.find(p_resource_id);
    if (resource == all_resources.end() || resource->second == MEM_NULL || resource->second->container == MEM_NULL) {
        throw std::runtime_error(std::format("RenderGraphVK: resource '{}' is not declared or has no container.", p_resource_id));
    }
    return resource->second->container;
}

void RenderGraphVK::instantiate_tasks(const RendGraphDescVK& p_desc) {
    Vector<RendTaskID, RenderAllocator> task_ids(render_data->allocator);
    task_ids.reserve(p_desc.tasks.size());
    for (const auto& task_desc : p_desc.tasks) {
        if (task_desc.task_id.empty()) {
            throw std::runtime_error("RenderGraphVK: task id is empty.");
        }
        if (std::find(task_ids.begin(), task_ids.end(), task_desc.task_id) != task_ids.end()) {
            throw std::runtime_error(std::format("RenderGraphVK: duplicate task id: {}.", task_desc.task_id));
        }
        task_ids.emplace_back(task_desc.task_id);
    }
    for (const auto& task_desc : p_desc.tasks) {
        if (!task_desc.create) {
            throw std::runtime_error(std::format("RenderGraphVK: task factory is missing for: {}.", task_desc.task_id));
        }
        auto task = task_desc.create();
        if (task.is_null()) {
            throw std::runtime_error("RenderGraphVK: task factory returned null.");
        }
        Ref<RGNode> node = add_node(std::move(task));
        node->task_id = task_desc.task_id;
        node->resource_map = task_desc.resource_map;
        node->render_task->bind_resources(RendResViewVK(&all_resources, &node->resource_map));
        node->render_task->append_accesses(node->reads, node->writes);
        normalize_task_accesses(node);
        node->resource_map.validate_all_used(node->task_id);
        for (const ResAccessVK& access : node->reads) {
            validate_declared_resource(all_resources, *node, access, false);
        }
        for (const ResAccessVK& access : node->writes) {
            validate_declared_resource(all_resources, *node, access, true);
        }
    }
}

void RenderGraphVK::normalize_task_accesses(Ref<RGNode> p_node) const {
    // Resolve task-local identifiers through the task's resource map, then expand aggregate resource lists.
    auto normalize = [this, &p_node](Vector<ResAccessVK, RenderAllocator>& p_accesses) {
        Vector<ResAccessVK, RenderAllocator> normalized(render_data->allocator);
        for (const ResAccessVK& access : p_accesses) {
            for (std::string& resource_id : expand_resource_id(p_node->resource_map.resolve(access.res_id))) {
                normalized.push_back({.res_id = std::move(resource_id), .usage = access.usage});
            }
        }
        p_accesses = std::move(normalized);
    };
    normalize(p_node->reads);
    normalize(p_node->writes);
}

Vector<std::string, RenderAllocator> RenderGraphVK::expand_resource_id(std::string_view p_resource_id) const {
    Vector<std::string, RenderAllocator> result(render_data->allocator);
    auto resource = all_resources.find(p_resource_id);
    if (resource == all_resources.end() || resource->second == MEM_NULL || resource->second->resource_list == MEM_NULL) {
        result.emplace_back(p_resource_id);
        return result;
    }
    for (size_t index = 0; index < resource->second->resource_list->get_resource_count(); ++index) {
        result.push_back(std::format("{}[{}]", p_resource_id, index));
    }
    return result;
}

void RenderGraphVK::add_resource_edges(const RendGraphDescVK& p_desc) {
    for (const RendTaskDescVK& task_desc : p_desc.tasks) {
        Ref<RGNode> from = find_node(task_desc.task_id);
        WBE_DEBUG_ASSERT(from != MEM_NULL);
        for (const RendResEdgeDescVK& edge_desc : task_desc.next) {
            Ref<RGNode> to = find_node(edge_desc.next_task_id);
            if (to == MEM_NULL) {
                throw std::runtime_error(std::format(
                    "RenderGraphVK: task '{}' connects to unknown task '{}'.", task_desc.task_id, edge_desc.next_task_id));
            }
            if (to == from) {
                throw std::runtime_error(std::format(
                    "RenderGraphVK: task '{}' connects to itself on resource '{}'.", task_desc.task_id, edge_desc.res_id));
            }
            if (!all_resources.contains(edge_desc.res_id)) {
                throw std::runtime_error(std::format("RenderGraphVK: edge '{}' -> '{}' uses undeclared resource '{}'.",
                    task_desc.task_id,
                    edge_desc.next_task_id,
                    edge_desc.res_id));
            }
            for (std::string& resource_id : expand_resource_id(edge_desc.res_id)) {
                add_resource_edge(from, to, std::move(resource_id), edge_desc.type, edge_desc.src_stage, edge_desc.dst_stage);
            }
        }
    }
}

void RenderGraphVK::validate_resource_edges() const {
    for (const Ref<RGResEdge>& edge : resource_edges) {
        validate_edge_stage(*edge->from, edge->src_stage, *edge, "source");
        validate_edge_stage(*edge->to, edge->dst_stage, *edge, "destination");
        if (edge->type == RendEdgeTypeVK::READ && node_accesses(edge->to->writes, edge->res_id)) {
            throw std::runtime_error(std::format("RenderGraphVK: edge '{}' -> '{}' on resource '{}' is a READ edge, but the next task writes the resource.",
                edge->from->task_id,
                edge->to->task_id,
                edge->res_id));
        }
    }
    // A chain of edges on one resource only orders its endpoints when consecutive edges share a stage at the middle task.
    for (Ref<RGNode> node : nodes) {
        for (auto& weak_in_edge : node->in_edges) {
            Ref<RGResEdge> in_edge = weak_in_edge.lock();
            WBE_DEBUG_ASSERT(in_edge != MEM_NULL);
            for (auto& weak_out_edge : node->out_edges) {
                Ref<RGResEdge> out_edge = weak_out_edge.lock();
                WBE_DEBUG_ASSERT(out_edge != MEM_NULL);
                if (in_edge->res_id != out_edge->res_id) {
                    continue;
                }
                if ((expand_stages(in_edge->dst_stage) & expand_stages(out_edge->src_stage)) == 0) {
                    throw std::runtime_error(std::format("RenderGraphVK: edges '{}' -> '{}' and '{}' -> '{}' on resource '{}' do not share a stage at task '{}'.",
                        in_edge->from->task_id,
                        node->task_id,
                        node->task_id,
                        out_edge->to->task_id,
                        in_edge->res_id,
                        node->task_id));
                }
            }
        }
    }
}

bool RenderGraphVK::has_resource_path(Ref<RGNode> p_from, Ref<RGNode> p_to, std::string_view p_resource_id) const {
    Vector<uint8_t, RenderAllocator> visited(nodes.size(), 0, render_data->allocator);
    Vector<Ref<RGNode>, RenderAllocator> stack(render_data->allocator);
    stack.push_back(p_from);
    visited[p_from->topo_order] = 1;
    while (!stack.empty()) {
        Ref<RGNode> node = stack.back();
        stack.pop_back();
        for (auto& weak_edge : node->out_edges) {
            Ref<RGResEdge> edge = weak_edge.lock();
            WBE_DEBUG_ASSERT(edge != MEM_NULL);
            if (edge->res_id != p_resource_id) {
                continue;
            }
            if (edge->to == p_to) {
                return true;
            }
            if (visited[edge->to->topo_order] != 0) {
                continue;
            }
            visited[edge->to->topo_order] = 1;
            stack.push_back(edge->to);
        }
    }
    return false;
}

void RenderGraphVK::validate_resource_conflicts() const {
    struct NodeAccess {
        Ref<RGNode> node;
        bool writes = false;
    };
    Map<std::string, Vector<NodeAccess, RenderAllocator>, RenderAllocator> accesses_by_resource(render_data->allocator);
    for (Ref<RGNode> node : nodes) {
        auto add_access = [&](const ResAccessVK& p_access, bool p_is_write) {
            auto [entry, inserted] = accesses_by_resource.try_emplace(p_access.res_id, render_data->allocator);
            Vector<NodeAccess, RenderAllocator>& accesses = entry->second;
            if (!accesses.empty() && accesses.back().node == node) {
                accesses.back().writes = accesses.back().writes || p_is_write;
                return;
            }
            auto existing = std::ranges::find_if(accesses, [&node](const NodeAccess& p_node_access) {
                return p_node_access.node == node;
            });
            if (existing != accesses.end()) {
                existing->writes = existing->writes || p_is_write;
                return;
            }
            accesses.push_back({.node = node, .writes = p_is_write});
        };
        for (const ResAccessVK& access : node->reads) {
            add_access(access, false);
        }
        for (const ResAccessVK& access : node->writes) {
            add_access(access, true);
        }
    }
    // Nodes are topologically sorted and were visited in order, so every access list is in execution order.
    for (const auto& [resource_id, accesses] : accesses_by_resource) {
        for (size_t before = 0; before < accesses.size(); ++before) {
            for (size_t after = before + 1; after < accesses.size(); ++after) {
                if (!accesses[before].writes && !accesses[after].writes) {
                    continue;
                }
                if (!has_resource_path(accesses[before].node, accesses[after].node, resource_id)) {
                    throw std::runtime_error(std::format("RenderGraphVK: tasks '{}' and '{}' both access resource '{}' without a chain of edges on that resource.",
                        accesses[before].node->task_id,
                        accesses[after].node->task_id,
                        resource_id));
                }
            }
        }
    }
}

void RenderGraphVK::validate_queue_completion() const {
    // The in-flight fence is attached to the last graphics submission. Work on another queue is only covered by it when a
    // dependency path reaches a graphics task.
    Vector<uint8_t, RenderAllocator> reaches_graphics(nodes.size(), 0, render_data->allocator);
    bool has_graphics = false;
    for (size_t i = nodes.size(); i > 0; --i) {
        Ref<RGNode> node = nodes[i - 1];
        if (node->in_queue == QueueFamilyVK::GRAPHICS) {
            has_graphics = true;
            reaches_graphics[i - 1] = 1;
            continue;
        }
        for (auto& weak_edge : node->out_edges) {
            Ref<RGResEdge> edge = weak_edge.lock();
            WBE_DEBUG_ASSERT(edge != MEM_NULL);
            if (reaches_graphics[edge->to->topo_order] != 0) {
                reaches_graphics[i - 1] = 1;
                break;
            }
        }
    }
    if (!has_graphics) {
        return;
    }
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (reaches_graphics[i] == 0 && !is_same_queue(nodes[i]->in_queue, QueueFamilyVK::GRAPHICS)) {
            throw std::runtime_error(std::format(
                "RenderGraphVK: task '{}' runs on another queue and has no dependency path to a graphics task.", nodes[i]->task_id));
        }
    }
}

bool RenderGraphVK::is_same_queue(QueueFamilyVK p_left, QueueFamilyVK p_right) const {
    return p_left == p_right || queue_for(render_data, p_left) == queue_for(render_data, p_right);
}

void RenderGraphVK::build_synchronization() {
    for (const Ref<RGResEdge>& edge : resource_edges) {
        Ref<RGNode> from = edge->from;
        Ref<RGNode> to = edge->to;
        if (is_same_queue(from->in_queue, to->in_queue)) {
            auto barrier = std::ranges::find_if(to->barriers, [&edge](const RGBarrierVK& p_barrier) {
                return p_barrier.res_id == edge->res_id;
            });
            if (barrier == to->barriers.end()) {
                to->barriers.push_back({.res_id = edge->res_id});
                barrier = to->barriers.end() - 1;
            }
            barrier->src_stage |= edge->src_stage;
            barrier->src_access |= VK_ACCESS_2_MEMORY_WRITE_BIT;
            barrier->dst_stage |= edge->dst_stage;
            barrier->dst_access |= edge_dst_access(edge->type);
            continue;
        }
        if (from->signal_semaphore == MEM_NULL) {
            from->signal_semaphore = make_ref<SemaphoreVK>(render_data->allocator, render_data, false);
        }
        from->signal_stage |= edge->src_stage;
        auto wait = std::ranges::find_if(to->semaphore_waits, [&from](const RGSemaphoreWaitVK& p_wait) {
            return p_wait.semaphore == from->signal_semaphore;
        });
        if (wait == to->semaphore_waits.end()) {
            to->semaphore_waits.push_back({.semaphore = from->signal_semaphore});
            wait = to->semaphore_waits.end() - 1;
        }
        wait->wait_stage |= edge->dst_stage;
    }
}

void RenderGraphVK::execute(uint64_t p_frame_index,
    double p_delta,
    std::span<const VkSemaphoreSubmitInfo> p_extra_waits_first_submit,
    std::span<const VkSemaphoreSubmitInfo> p_extra_signals_last_submit) {
    if (nodes.empty()) {
        return;
    }
    const uint64_t timeline_value = p_frame_index + 1;
    const auto [first_graphics_idx, last_graphics_idx] = find_graphics_range();

    // Each graph node owns one command buffer per frame slot and is submitted independently.
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i];
        auto queue_family = node->in_queue;
        const uint32_t frame_slot = render_data->current_frame_slot;
        const size_t command_buffer_index = static_cast<size_t>(frame_slot) * nodes.size() + i;
        VkCommandBuffer command_buffer = command_buffers[command_buffer_index];

        if (vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS) {
            throw std::runtime_error("RenderGraphVK: failed to reset command buffer.");
        }

        RendResViewVK resource_view(&all_resources);
        const bool is_first_graphics = (i == first_graphics_idx && queue_family == QueueFamilyVK::GRAPHICS);
        const bool is_last_graphics = (i == last_graphics_idx && queue_family == QueueFamilyVK::GRAPHICS);
        // The first/last graphics nodes bridge external acquire/present synchronization.
        record_node(node, command_buffer, resource_view, p_delta, is_last_graphics);

        // Build wait/signal semaphore lists from cross-queue resource edges + extras.
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> wait_infos(render_data->allocator);
        Vector<VkSemaphoreSubmitInfo, RenderAllocator> signal_infos(render_data->allocator);
        collect_node_semaphores(node, timeline_value, wait_infos, signal_infos);

        if (is_first_graphics) {
            for (const auto& wait_info : p_extra_waits_first_submit) {
                wait_infos.push_back(wait_info);
            }
        }
        if (is_last_graphics) {
            for (const auto& signal_info : p_extra_signals_last_submit) {
                signal_infos.push_back(signal_info);
            }
            // Also signal the terminal binary semaphore for this swap-chain image.
            signal_infos.push_back(get_terminal_semaphore()->get_submit_info(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT));
        }

        // Last graphics submit signals the in-flight fence too, so the renderer
        // may wait for the entire frame's GPU work to complete next time
        // around.
        VkFence fence = is_last_graphics ? render_data->in_flight_fence : VK_NULL_HANDLE;
        submit_node(command_buffer, queue_family, wait_infos, signal_infos, fence);
    }
}

std::tuple<size_t, size_t> RenderGraphVK::find_graphics_range() const {
    size_t first_index = nodes.size();
    size_t last_index = nodes.size();
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i]->in_queue == QueueFamilyVK::GRAPHICS) {
            if (first_index == nodes.size()) {
                first_index = i;
            }
            last_index = i;
        }
    }
    return {first_index, last_index};
}

void RenderGraphVK::record_node(
    Ref<RGNode> p_node, VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta, bool p_record_present_transition) {
    // Record all barriers and task work for one node into its per-frame primary command buffer.
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(p_command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: failed to begin command buffer.");
    }
    if (render_data->descriptor_heap_allocator == nullptr) {
        throw std::runtime_error("RenderGraphVK: descriptor heap allocator is unavailable.");
    }
    render_data->descriptor_heap_allocator->bind_heaps(
        p_command_buffer, render_data->resource_descriptor_heap_id, render_data->sampler_descriptor_heap_id);

    record_general_transition(p_command_buffer, p_node, p_resources);
    record_edge_barriers(p_command_buffer, p_node, p_resources);

    p_node->render_task->record(p_command_buffer, p_delta);

    if (p_record_present_transition) {
        // Present waits on this node's terminal semaphore, so the present layout transition must be recorded before it is signaled.
        record_present_transition(p_command_buffer);
    }

    if (vkEndCommandBuffer(p_command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("RenderGraphVK: failed to end command buffer.");
    }
}

void RenderGraphVK::record_general_transition(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const RendResViewVK& p_resources) {
    // We're already using unified shader layout, but we still need an explicit transition out of UNDEFINED/PRESENT.
    QueueFamilyVK queue_family = node_queue_family(p_node);
    for (const auto& write : p_node->writes) {
        const auto& bound_resource = p_resources.get(write.res_id);
        auto image_view = resolve_render_object(bound_resource, render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>();
        if (image_view == MEM_NULL || image_view->fields().image == MEM_NULL) {
            continue;
        }
        Ref<ROImageVK> image = image_view->fields().image;
        if (image->fields().image_layout == VK_IMAGE_LAYOUT_GENERAL) {
            // Filter out images that are already in general layout.
            continue;
        }
        ResourceScopeVK write_scope = resource_scope(write.usage, true, queue_family);
        // The source stage chains the transition after semaphore waits at the write stage, such as swap-chain acquisition.
        ROImageVK::MemoryBarrierBuilder()
            .set_src_access(VK_ACCESS_2_NONE, write_scope.stage)
            .set_dst_access(write_scope.access, write_scope.stage)
            .set_subresource_range(image_view->fields().subresource_range)
            .set_new_layout(VK_IMAGE_LAYOUT_GENERAL)
            .set_image(image)
            .apply_barrier(p_command_buffer);
    }
}

void RenderGraphVK::record_edge_barriers(VkCommandBuffer p_command_buffer, Ref<RGNode> p_node, const RendResViewVK& p_resources) const {
    if (p_node->barriers.empty()) {
        return;
    }
    VkMemoryBarrier2 memory_barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    Vector<VkBufferMemoryBarrier2, RenderAllocator> buffer_barriers(render_data->allocator);
    Vector<VkImageMemoryBarrier2, RenderAllocator> image_barriers(render_data->allocator);
    for (const RGBarrierVK& barrier : p_node->barriers) {
        Ref<IRenderObjectVK> resource = resolve_render_object(p_resources.get(barrier.res_id), render_data->current_frame_slot);
        if (auto buffer = resource.dynamic_cast_ref<ROBufferVK>(); buffer != MEM_NULL) {
            buffer_barriers.push_back(VkBufferMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = barrier.src_stage,
                .srcAccessMask = barrier.src_access,
                .dstStageMask = barrier.dst_stage,
                .dstAccessMask = barrier.dst_access,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = buffer->get_buffer(),
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            });
            continue;
        }
        auto image_view = resource.dynamic_cast_ref<ROImageViewVK>();
        if (image_view != MEM_NULL && image_view->fields().image != MEM_NULL &&
            image_view->fields().image->fields().image_layout == VK_IMAGE_LAYOUT_GENERAL) {
            // Different views may alias the same image, so the barrier covers every mip level and layer.
            image_barriers.push_back(VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = barrier.src_stage,
                .srcAccessMask = barrier.src_access,
                .dstStageMask = barrier.dst_stage,
                .dstAccessMask = barrier.dst_access,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image_view->fields().image->get_image(),
                .subresourceRange =
                    VkImageSubresourceRange{
                        .aspectMask = image_view->fields().subresource_range.aspectMask,
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    },
            });
            continue;
        }
        // Other resources, and images outside the unified layout, are covered by a global memory barrier.
        memory_barrier.srcStageMask |= barrier.src_stage;
        memory_barrier.srcAccessMask |= barrier.src_access;
        memory_barrier.dstStageMask |= barrier.dst_stage;
        memory_barrier.dstAccessMask |= barrier.dst_access;
    }
    const bool has_memory_barrier = memory_barrier.srcStageMask != VK_PIPELINE_STAGE_2_NONE;
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = has_memory_barrier ? 1U : 0U,
        .pMemoryBarriers = has_memory_barrier ? &memory_barrier : nullptr,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size()),
        .pBufferMemoryBarriers = buffer_barriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size()),
        .pImageMemoryBarriers = image_barriers.data(),
    };
    vkCmdPipelineBarrier2(p_command_buffer, &dependency_info);
}

void RenderGraphVK::record_present_transition(VkCommandBuffer p_command_buffer) {
    // WSI presentation still requires PRESENT_SRC_KHR even when unified image layouts are enabled.
    RendererVK* renderer = RendererVK::get_singleton();
    if (renderer == nullptr) {
        throw std::runtime_error("RenderGraphVK: renderer singleton is unavailable.");
    }
    Ref<ROCMultislotsVK> swap_chain_image_views = renderer->acquire_swap_chain_image_view_container();
    auto image_view = swap_chain_image_views->get(render_data->current_frame_slot).dynamic_cast_ref<ROImageViewVK>();
    if (image_view == MEM_NULL || image_view->fields().image == MEM_NULL) {
        throw std::runtime_error("RenderGraphVK: swap-chain image-view container did not resolve to an image view.");
    }

    Ref<ROImageVK> image = image_view->fields().image;
    if (!image->fields().is_swap_chain_image) {
        throw std::runtime_error("RenderGraphVK: swap-chain image resource does not reference a swap-chain image.");
    }

    // For the swapchain image, apply a barrier that transfers it into VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
    ROImageVK::MemoryBarrierBuilder()
        .set_src_access(VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT)
        .set_dst_access(VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE)
        .set_subresource_range(image_view->fields().subresource_range)
        .set_new_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        .set_image(image)
        .apply_barrier(p_command_buffer);
}

void RenderGraphVK::collect_node_semaphores(Ref<RGNode> p_node,
    uint64_t p_timeline_value,
    Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_waits,
    Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_signals) const {
    for (const RGSemaphoreWaitVK& wait : p_node->semaphore_waits) {
        p_waits.push_back(wait.semaphore->get_submit_info(wait.wait_stage, p_timeline_value));
    }
    if (p_node->signal_semaphore != MEM_NULL) {
        p_signals.push_back(p_node->signal_semaphore->get_submit_info(p_node->signal_stage, p_timeline_value));
    }
}

void RenderGraphVK::submit_node(VkCommandBuffer p_command_buffer,
    QueueFamilyVK p_queue_family,
    const Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_waits,
    const Vector<VkSemaphoreSubmitInfo, RenderAllocator>& p_signals,
    VkFence p_fence) {
    VkCommandBufferSubmitInfo command_buffer_submit_info{};
    command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer = p_command_buffer;

    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(p_waits.size());
    submit_info.pWaitSemaphoreInfos = p_waits.data();
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_submit_info;
    submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(p_signals.size());
    submit_info.pSignalSemaphoreInfos = p_signals.data();

    VkQueue queue = queue_for(render_data, p_queue_family);
    auto result = vkQueueSubmit2(queue, 1, &submit_info, p_fence);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("RenderGraphVK: vkQueueSubmit2 failed: {}", string_VkResult(result)));
    }
}

} // namespace WhiteBirdEngine
