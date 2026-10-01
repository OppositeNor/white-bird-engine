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
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_task.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render/render_task_resource_map.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/roc_single_vk.hh"
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "function/render_vk/i_mesh_compiler_vk.hh"
#include "function/render_vk/static_mesh_resources_vk.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator) : next(p_allocator), resource_map(p_allocator) {
}

RendTaskDescVK::RendTaskDescVK(RenderAllocator* p_allocator, std::string_view p_task_id, Unique<IRenderTaskVK> p_task)
    : task_id(p_task_id), next(p_allocator), resource_map(p_allocator), task(std::move(p_task)) {
    if (task.is_null()) {
        throw std::runtime_error("Failed to construct render task descriptor: task is null.");
    }
}

RendTaskDescVK& RendTaskDescVK::connect(std::string_view p_next_task_id, std::string_view p_resource_id) {
    next.push_back({
        .next_task_id = std::string(p_next_task_id),
        .res_id = std::string(p_resource_id),
    });
    return *this;
}

RendTaskDescVK& RendTaskDescVK::alias(std::string_view p_task_resource_id, std::string_view p_graph_resource_id) {
    resource_map.bind(p_task_resource_id, p_graph_resource_id);
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(RendTaskDescVK p_task_desc) {
    tasks.emplace_back(std::move(p_task_desc));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(std::string_view p_task_id, Unique<IRenderTaskVK> p_task) {
    tasks.emplace_back(allocator, p_task_id, std::move(p_task));
    return *this;
}

RendTaskDescVK& RendGraphDescVK::get_task(std::string_view p_task_id) {
    for (RendTaskDescVK& task : tasks) {
        if (task.task_id == p_task_id) {
            return task;
        }
    }
    throw std::runtime_error(std::format("Failed to get render graph task: no task has id '{}'.", p_task_id));
}

RendGraphDescVK& RendGraphDescVK::add_resource(RendResVK p_resource) {
    if (p_resource.container != MEM_NULL) {
        RendResTID concrete_type_id = p_resource.container->get_resource_type_id();
        if (concrete_type_id == 0) {
            throw std::runtime_error("Failed to add render graph resource: container has no concrete resource type.");
        }
        if (p_resource.res_type_id != 0 && p_resource.res_type_id != concrete_type_id) {
            throw std::runtime_error("Failed to add render graph resource: container type does not match the declared type.");
        }
        p_resource.res_type_id = concrete_type_id;
    }
    resources.emplace_back(std::move(p_resource));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, Ref<IRenderObjectVK> p_resource) {
    if (p_resource == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to add render graph resource: render object for slot: {} is null.", p_resource_id));
    }
    return add_resource(p_resource_id, make_ref<ROCSingleVK>(allocator, std::move(p_resource)));
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, RendResTID p_resource_type_id) {
    return add_resource(RendResVK{.res_id = std::string(p_resource_id), .res_type_id = p_resource_type_id, .container = MEM_NULL});
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, Ref<RenderObjectContainerVK> p_resource_container) {
    if (p_resource_container == MEM_NULL) {
        throw std::runtime_error("Failed to add render graph resource: resource container is null.");
    }
    return add_resource(RendResVK{
        .res_id = std::string(p_resource_id),
        .res_type_id = p_resource_container->get_resource_type_id(),
        .container = std::move(p_resource_container),
    });
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, Ref<RenderResourceList> p_resource_list) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph resource list: resource id is empty.");
    }
    if (p_resource_list == MEM_NULL) {
        throw std::runtime_error("Failed to add render graph resource list: resource list is null.");
    }
    return add_resource(RendResVK{
        .res_id = std::string(p_resource_id),
        .resource_list = std::move(p_resource_list),
    });
}

RendGraphDescVK& RendGraphDescVK::add_buffer(std::string_view p_resource_id, RendBufferDescVK p_desc) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph buffer: resource id is empty.");
    }
    buffers.push_back({.res_id = std::string(p_resource_id), .desc = std::move(p_desc)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image(std::string_view p_resource_id, RendImageDescVK p_desc) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph image: resource id is empty.");
    }
    images.push_back({.res_id = std::string(p_resource_id), .desc = p_desc});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image_view(std::string_view p_resource_id, std::string_view p_image_id, RendImageViewDescVK p_desc) {
    if (p_resource_id.empty() || p_image_id.empty()) {
        throw std::runtime_error("Failed to add render graph image view: resource id or image id is empty.");
    }
    image_views.push_back({.res_id = std::string(p_resource_id), .image_id = std::string(p_image_id), .desc = p_desc});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_sampler(std::string_view p_resource_id, const RendSamplerDescVK& p_desc) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph sampler: resource id is empty.");
    }
    samplers.push_back({.res_id = std::string(p_resource_id), .desc = p_desc});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_static_mesh(Ref<StaticMeshResourcesVK> p_static_mesh) {
    if (p_static_mesh == MEM_NULL) {
        throw std::runtime_error("Failed to add static mesh to render graph: static mesh is null.");
    }
    add_resource(REND_RES_STATIC_MESH_VERTICES, p_static_mesh->acquire_vertices())
        .add_resource(REND_RES_STATIC_MESH_INDICES, p_static_mesh->acquire_indices())
        .add_resource(REND_RES_STATIC_MESH_SUBMESHES, p_static_mesh->acquire_submeshes())
        .add_resource(REND_RES_STATIC_MESH_NODES, p_static_mesh->acquire_nodes())
        .add_resource(REND_RES_STATIC_MESH_INSTANCES, p_static_mesh->acquire_instances())
        .add_resource(REND_RES_STATIC_MESH_TEXTURES, p_static_mesh->acquire_textures());
    if (p_static_mesh->acquire_opaque_draw_batches() != MEM_NULL) {
        add_resource(REND_RES_STATIC_MESH_OPAQUE_DRAW_BATCHES, p_static_mesh->acquire_opaque_draw_batches());
    }
    if (p_static_mesh->acquire_opaque_double_sided_draw_batches() != MEM_NULL) {
        add_resource(REND_RES_STATIC_MESH_OPAQUE_DOUBLE_SIDED_DRAW_BATCHES, p_static_mesh->acquire_opaque_double_sided_draw_batches());
    }
    if (p_static_mesh->acquire_masked_draw_batches() != MEM_NULL) {
        add_resource(REND_RES_STATIC_MESH_MASKED_DRAW_BATCHES, p_static_mesh->acquire_masked_draw_batches());
    }
    if (p_static_mesh->acquire_material_params() != MEM_NULL) {
        add_resource(p_static_mesh->get_material_params_resource_id(), p_static_mesh->acquire_material_params());
    }
    if (p_static_mesh->acquire_material_sampler() != MEM_NULL) {
        add_resource(REND_RES_MESH_MATERIAL_SAMPLER, p_static_mesh->acquire_material_sampler());
    }
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_dynamic_mesh(Ref<DynamicMeshResourcesVK> p_dynamic_mesh) {
    if (p_dynamic_mesh == MEM_NULL) {
        throw std::runtime_error("Failed to add dynamic mesh to render graph: dynamic mesh is null.");
    }
    add_resource(REND_RES_DYNAMIC_MESH_VERTICES, p_dynamic_mesh->acquire_vertices())
        .add_resource(REND_RES_DYNAMIC_MESH_INDICES, p_dynamic_mesh->acquire_indices())
        .add_resource(REND_RES_DYNAMIC_MESH_DRAW_BATCHES, p_dynamic_mesh->acquire_draw_batches())
        .add_resource(REND_RES_DYNAMIC_MESH_SUBMESHES, p_dynamic_mesh->acquire_submeshes())
        .add_resource(REND_RES_DYNAMIC_MESH_NODES, p_dynamic_mesh->acquire_nodes())
        .add_resource(REND_RES_DYNAMIC_MESH_INSTANCES, p_dynamic_mesh->acquire_instances())
        .add_resource(REND_RES_DYNAMIC_MESH_TEXTURES, p_dynamic_mesh->acquire_textures());
    if (p_dynamic_mesh->acquire_material_params() != MEM_NULL) {
        add_resource(REND_RES_DYNAMIC_MESH_MATERIAL_PARAMS, p_dynamic_mesh->acquire_material_params());
    }
    if (p_dynamic_mesh->acquire_material_sampler() != MEM_NULL) {
        add_resource(REND_RES_DYNAMIC_MESH_MATERIAL_SAMPLER, p_dynamic_mesh->acquire_material_sampler());
    }
    return *this;
}

} // namespace WhiteBirdEngine
