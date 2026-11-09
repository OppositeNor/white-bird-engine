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
#include "function/render_vk/rr_mesh_vk.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_object.hh"
#include "function/render_vk/renderer_vk.hh"
#include "generated/serializables_sd.gen.hh"
#include "generated/type_uuid.gen.hh"
#include "resource/resource_manager/resource_manager.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/mesh_compiler_vk.hh"
#include "function/render_vk/mesh_resources_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"

namespace WhiteBirdEngine {

namespace {

RendererVK* acquire_renderer(std::string_view p_key) {
    RendererVK* renderer = RendererVK::get_singleton();
    if (renderer == nullptr) {
        throw std::runtime_error(std::format("Failed to register mesh '{}': the Vulkan renderer is unavailable.", p_key));
    }
    return renderer;
}

template <typename TResource>
RefRes<TResource> load_source(std::string_view p_key, std::string_view p_source_key) {
    ResourceManager* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr) {
        throw std::runtime_error(std::format("Failed to register mesh '{}': the resource manager is unavailable.", p_key));
    }
    RefRes<TResource> resource = resource_manager->get_resource<TResource>(std::string(p_source_key));
    if (resource == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to register mesh '{}': no resource has the key '{}'.", p_key, p_source_key));
    }
    return resource;
}

} // namespace

RRMeshVK::RRMeshVK() = default;

RRMeshVK::~RRMeshVK() = default;

TypeID RRMeshVK::get_description_type_uuid() const {
    return typed_description_type_uuid();
}

Unique<IResourceDescription> RRMeshVK::deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) const {
    return typed_deserialize_description(p_description, p_allocator);
}

Ref<IRenderObject> RRMeshVK::compile_typed(IRenderGraph& /* p_graph */, std::string_view p_key, const RDParamsMeshVK& p_params) {
    RendererVK* renderer = acquire_renderer(p_key);
    if (p_params.mesh.empty()) {
        throw std::runtime_error(std::format("Failed to register mesh '{}': the mesh resource key is empty.", p_key));
    }
    // Fail with the registration key before the compiler reports the source key.
    static_cast<void>(load_source<RUMesh3D>(p_key, p_params.mesh));
    MeshCompilerVK::Config config;
    config.default_material_resource_key = p_params.default_material;
    config.vertex_buffer_usage = to_vk_buffer_usage_flags(p_params.vertex_buffer_usage);
    config.index_buffer_usage = to_vk_buffer_usage_flags(p_params.index_buffer_usage);
    return MeshCompilerVK(renderer).acquire(p_params.mesh, config);
}

} // namespace WhiteBirdEngine
