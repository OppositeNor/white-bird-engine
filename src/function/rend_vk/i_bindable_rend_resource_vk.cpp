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
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include "core/memory/reference_strong.hh"
#include "function/rend_vk/rend_resource_list_vk.hh"
#include "function/render/i_rend_object.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/render/i_rend_object_container.hh"
#include "function/render/roc_single.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

std::string IBindableRendResourceVK::make_member_resource_id(std::string_view p_resource_id, std::string_view p_member) {
    return std::format("{}.{}", p_resource_id, p_member);
}

void IBindableRendResourceVK::bind_rend_object(
    RendGraphDescVK& p_graph, std::string_view p_resource_id, Ref<IRendObject> p_resource) {
    if (p_resource == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind render resource '{}': render object is null.", p_resource_id));
    }
    if (p_resource->get_debug_name().empty()) {
        // Objects owned outside every registry would otherwise stay unnamed; a name given elsewhere is kept.
        p_resource->set_debug_name(p_resource_id);
    }
    // The container keeps resolving the imported object; recreating it rebinds the same object.
    bind_container(p_graph, p_resource_id, make_ref<ROCSingle>(p_graph.alloc, [resource = std::move(p_resource)](uint32_t /*p_slot*/) -> Ref<IRendObject> {
        return resource;
    }));
}

void IBindableRendResourceVK::bind_container(
    RendGraphDescVK& p_graph, std::string_view p_resource_id, Ref<IRendObjectContainer> p_container) {
    p_graph.add_resource(p_resource_id, std::move(p_container));
}

void IBindableRendResourceVK::bind_resource_list(
    RendGraphDescVK& p_graph, std::string_view p_resource_id, Ref<RendResourceListVK> p_resource_list) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to bind render resource list: resource id is empty.");
    }
    if (p_resource_list == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind render resource list '{}': resource list is null.", p_resource_id));
    }
    p_graph.add_resource(RendResVK{
        .res_id = std::string(p_resource_id),
        .resource_list = std::move(p_resource_list),
    });
}

} // namespace WhiteBirdEngine
