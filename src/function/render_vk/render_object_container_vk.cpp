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
#include "function/render_vk/render_object_container_vk.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/parser/parser_json.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include <sstream>
#include <stdexcept>

namespace WhiteBirdEngine {

namespace {

std::string descriptor_rows_to_string(const DebugDataRows& p_rows) {
    DebugDataArray rows;
    rows.reserve(p_rows.size());
    for (const DebugData& row : p_rows) {
        rows.push_back(row);
    }
    DebugData root;
    root.set(std::move(rows));
    std::ostringstream stream;
    stream << root;
    return stream.str();
}

} // namespace

std::string RenderObjectContainerVK::to_string() const {
    return descriptor_rows_to_string(descriptor_debug_data());
}

RendResTID RenderObjectContainerVK::get_resource_type_id() const {
    return resource_type_id;
}

void RenderObjectContainerVK::retain_resource_type(Ref<IRenderObjectVK> p_resource) {
    if (p_resource == MEM_NULL) {
        return;
    }
    RendResTID concrete_type_id = p_resource->get_type_uuid();
    if (resource_type_id == 0) {
        resource_type_id = concrete_type_id;
        return;
    }
    if (resource_type_id != concrete_type_id) {
        throw std::runtime_error("Render object container resources must have the same concrete type.");
    }
}

} // namespace WhiteBirdEngine
