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
#include "function/render/global_render_cache.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

GlobalRenderCache::GlobalRenderCache(AllocType* p_allocator)
    : cache(p_allocator) {
    singleton = this;
}

void GlobalRenderCache::insert(std::string_view p_resource_key, Ref<IRenderObject> p_render_object) {
    if (p_resource_key.empty()) {
        throw std::runtime_error("Failed to cache compiled render object: the resource key is empty.");
    }
    if (p_render_object == MEM_NULL) {
        throw std::runtime_error("Failed to cache compiled render object: the render object is null.");
    }
    cache.insert(Key{.resource_key = std::string(p_resource_key), .render_object_type_id = p_render_object->get_type_uuid()},
        p_render_object);
    p_render_object->set_debug_name(p_resource_key);
}

Ref<IRenderObject> GlobalRenderCache::try_get(std::string_view p_resource_key, TypeID p_render_object_type_id) const {
    return cache.try_get(KeyView{.resource_key = p_resource_key, .render_object_type_id = p_render_object_type_id});
}

bool GlobalRenderCache::contains(std::string_view p_resource_key, TypeID p_render_object_type_id) const {
    return cache.contains(KeyView{.resource_key = p_resource_key, .render_object_type_id = p_render_object_type_id});
}

size_t GlobalRenderCache::prune_expired() {
    return cache.prune_expired();
}

void GlobalRenderCache::clear() {
    cache.clear();
}

} // namespace WhiteBirdEngine
