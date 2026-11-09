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
#include "function/render_vk/rr_compute_pipeline_vk.hh"
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
#include "function/render_vk/compute_pipeline_compiler_vk.hh"
#include "function/render_vk/render_context_vk.hh"

namespace WhiteBirdEngine {

namespace {

RendererVK* acquire_renderer(std::string_view p_key) {
    RendererVK* renderer = RendererVK::get_singleton();
    if (renderer == nullptr) {
        throw std::runtime_error(std::format("Failed to register compute pipeline '{}': the Vulkan renderer is unavailable.", p_key));
    }
    return renderer;
}

template <typename TResource>
RefRes<TResource> load_source(std::string_view p_key, std::string_view p_source_key) {
    ResourceManager* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr) {
        throw std::runtime_error(std::format("Failed to register compute pipeline '{}': the resource manager is unavailable.", p_key));
    }
    RefRes<TResource> resource = resource_manager->get_resource<TResource>(std::string(p_source_key));
    if (resource == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to register compute pipeline '{}': no resource has the key '{}'.", p_key, p_source_key));
    }
    return resource;
}

} // namespace

RRComputePipelineVK::RRComputePipelineVK() = default;

RRComputePipelineVK::~RRComputePipelineVK() = default;

TypeID RRComputePipelineVK::get_description_type_uuid() const {
    return typed_description_type_uuid();
}

Unique<IResourceDescription> RRComputePipelineVK::deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) const {
    return typed_deserialize_description(p_description, p_allocator);
}

Ref<IRenderObject> RRComputePipelineVK::compile_typed(
    IRenderGraph& /* p_graph */, std::string_view p_key, const RDParamsComputePipelineVK& p_params) {
    RendererVK* renderer = acquire_renderer(p_key);
    RefRes<RUShader> shader = load_source<RUShader>(p_key, p_params.shader);
    const RenderContextVK* render_data = renderer->get_render_context();
    ComputePipelineCompilerVKContext context{
        .allocator = render_data->allocator,
        .render_data = render_data,
        .render_cache = renderer->get_global_render_cache(),
    };
    return ComputePipelineCompilerVK(context).compile(p_key, shader).compute_pipeline;
}

} // namespace WhiteBirdEngine
