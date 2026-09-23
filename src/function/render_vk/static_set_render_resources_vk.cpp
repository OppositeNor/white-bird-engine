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
#include "function/render_vk/static_set_render_resources_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include <cstddef>
#include <stdexcept>

namespace WhiteBirdEngine {

StaticSetRenderResourcesVK::StaticSetRenderResourcesVK(RenderAllocator* p_allocator)
    : allocator(p_allocator), textures(make_ref<RenderResourceList>(p_allocator, p_allocator)), opaque_batches(p_allocator),
      masked_batches(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to construct static-set render resources: allocator is null.");
    }
}

const StaticSetRenderResourcesVK::DrawBatch& StaticSetRenderResourcesVK::get_opaque_draw_batch(size_t p_index) const {
    if (p_index >= opaque_batches.size()) {
        throw std::runtime_error("Failed to get static-set opaque draw batch: index is out of bounds.");
    }
    return opaque_batches[p_index];
}

const StaticSetRenderResourcesVK::DrawBatch& StaticSetRenderResourcesVK::get_masked_draw_batch(size_t p_index) const {
    if (p_index >= masked_batches.size()) {
        throw std::runtime_error("Failed to get static-set masked draw batch: index is out of bounds.");
    }
    return masked_batches[p_index];
}

RendTaskDescVK& StaticSetRenderResourcesVK::register_resources(RendTaskDescVK& p_task_desc) const {
    if (vertices != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_VERTICES, vertices);
    }
    if (indices != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_INDICES, indices);
    }
    if (opaque_draw_batches != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_OPAQUE_DRAW_BATCHES, opaque_draw_batches);
    }
    if (masked_draw_batches != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_MASKED_DRAW_BATCHES, masked_draw_batches);
    }
    if (submeshes != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_SUBMESHES, submeshes).read(REND_RES_STATIC_SET_SUBMESHES, RendResUsageVK::STORAGE_BUFFER);
    }
    if (nodes != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_NODES, nodes).read(REND_RES_STATIC_SET_NODES, RendResUsageVK::STORAGE_BUFFER);
    }
    if (instances != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_INSTANCES, instances).read(REND_RES_STATIC_SET_INSTANCES, RendResUsageVK::STORAGE_BUFFER);
    }
    if (material_params != MEM_NULL) {
        p_task_desc.add_resource(material_params_resource_id, material_params).read(material_params_resource_id, RendResUsageVK::UNIFORM_BUFFER);
    }
    if (material_sampler != MEM_NULL) {
        p_task_desc.add_resource(REND_RES_STATIC_SET_MATERIAL_SAMPLER, material_sampler);
    }
    p_task_desc.add_resource(REND_RES_STATIC_SET_TEXTURES, textures);
    if (textures->get_resource_count() > 0) {
        p_task_desc.read(REND_RES_STATIC_SET_TEXTURES, RendResUsageVK::SAMPLED_IMAGE);
    }
    return p_task_desc;
}

} // namespace WhiteBirdEngine
