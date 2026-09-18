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
#include "function/render_vk/roc_single_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

ROCSingleVK::ROCSingleVK(Ref<IRenderObjectVK> p_resource) : resource(std::move(p_resource)) {
    if (resource == MEM_NULL) {
        throw std::runtime_error("ROCSingleVK: resource is null.");
    }
    retain_resource_type(resource);
}

Ref<IRenderObject> ROCSingleVK::get() {
    return resource;
}

Ref<IRenderObject> ROCSingleVK::get() const {
    return resource;
}

Ref<IRenderObject> ROCSingleVK::get(uint32_t /*p_frame_slot*/) {
    return resource;
}

Ref<IRenderObject> ROCSingleVK::get(uint32_t /*p_frame_slot*/) const {
    return resource;
}

DebugDataRows ROCSingleVK::descriptor_debug_data() const {
    WBE_DEBUG_ASSERT(resource != MEM_NULL);
    return resource->descriptor_debug_data();
}

} // namespace WhiteBirdEngine
