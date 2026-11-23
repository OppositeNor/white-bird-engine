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
#include "function/render/roc_single.hh"
#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/debug_data.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace WhiteBirdEngine {

ROCSingle::ROCSingle(Factory p_factory) : factory(std::move(p_factory)) {
    if (factory == nullptr) {
        throw std::runtime_error("ROCSingle: resource factory is null.");
    }
    recreate();
}

void ROCSingle::recreate() {
    Ref<IRendObject> new_resource = factory(0);
    if (new_resource == MEM_NULL) {
        throw std::runtime_error("ROCSingle: resource factory returned null.");
    }
    retain_resource_type(new_resource);
    resource = std::move(new_resource);
}

void ROCSingle::set(uint32_t p_frame_slot, Ref<IRendObject> p_resource) {
    if (p_frame_slot != 0) {
        throw std::runtime_error("ROCSingle: slot is out of range.");
    }
    resource = std::move(p_resource);
    retain_resource_type(resource);
}

void ROCSingle::clear(uint32_t p_frame_slot) {
    if (p_frame_slot != 0) {
        throw std::runtime_error("ROCSingle: slot is out of range.");
    }
    resource = MEM_NULL;
}

void ROCSingle::clear() {
    clear(0);
}

void ROCSingle::set(Ref<IRendObject> p_resource) {
    set(0, std::move(p_resource));
}

Ref<IRendObject> ROCSingle::get() {
    return resource;
}

Ref<IRendObject> ROCSingle::get() const {
    return resource;
}

Ref<IRendObject> ROCSingle::get(uint32_t /*p_frame_slot*/) {
    return resource;
}

Ref<IRendObject> ROCSingle::get(uint32_t /*p_frame_slot*/) const {
    return resource;
}

uint32_t ROCSingle::get_slot_count() const {
    return 1;
}

DebugDataRows ROCSingle::desc_debug_data(RendAlloc* p_alloc) const {
    WBE_DEBUG_ASSERT(resource != MEM_NULL);
    return resource->desc_debug_data(p_alloc);
}

} // namespace WhiteBirdEngine
