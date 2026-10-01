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
#include "function/render_vk/static_mesh_resources_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include <cstddef>
#include <stdexcept>

namespace WhiteBirdEngine {

StaticMeshResourcesVK::StaticMeshResourcesVK(RenderAllocator* p_allocator)
    : allocator(p_allocator), textures(make_ref<RenderResourceList>(p_allocator, p_allocator)), opaque_batches(p_allocator),
      opaque_double_sided_batches(p_allocator), masked_batches(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Failed to construct static-mesh resources: allocator is null.");
    }
}

const MeshDrawBatchVK& StaticMeshResourcesVK::get_opaque_draw_batch(size_t p_index) const {
    if (p_index >= opaque_batches.size()) {
        throw std::runtime_error("Failed to get static-mesh opaque draw batch: index is out of bounds.");
    }
    return opaque_batches[p_index];
}

const MeshDrawBatchVK& StaticMeshResourcesVK::get_opaque_double_sided_draw_batch(size_t p_index) const {
    if (p_index >= opaque_double_sided_batches.size()) {
        throw std::runtime_error("Failed to get static-mesh opaque_double_sided draw batch: index is out of bounds.");
    }
    return opaque_double_sided_batches[p_index];
}

const MeshDrawBatchVK& StaticMeshResourcesVK::get_masked_draw_batch(size_t p_index) const {
    if (p_index >= masked_batches.size()) {
        throw std::runtime_error("Failed to get static-mesh masked draw batch: index is out of bounds.");
    }
    return masked_batches[p_index];
}

} // namespace WhiteBirdEngine
