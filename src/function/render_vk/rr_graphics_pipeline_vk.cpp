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
#include "function/render_vk/rr_graphics_pipeline_vk.hh"
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
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include "function/render_vk/vulkan_enums_vk.hh"
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

RendererVK* acquire_renderer(std::string_view p_key) {
    RendererVK* renderer = RendererVK::get_singleton();
    if (renderer == nullptr) {
        throw std::runtime_error(std::format("Failed to register graphics pipeline '{}': the Vulkan renderer is unavailable.", p_key));
    }
    return renderer;
}

template <typename TResource>
RefRes<TResource> load_source(std::string_view p_key, std::string_view p_source_key) {
    ResourceManager* resource_manager = ResourceManager::get_singleton();
    if (resource_manager == nullptr) {
        throw std::runtime_error(std::format("Failed to register graphics pipeline '{}': the resource manager is unavailable.", p_key));
    }
    RefRes<TResource> resource = resource_manager->get_resource<TResource>(std::string(p_source_key));
    if (resource == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to register graphics pipeline '{}': no resource has the key '{}'.", p_key, p_source_key));
    }
    return resource;
}

} // namespace

RRGraphicsPipelineVK::RRGraphicsPipelineVK() = default;

RRGraphicsPipelineVK::~RRGraphicsPipelineVK() = default;

TypeID RRGraphicsPipelineVK::get_description_type_uuid() const {
    return typed_description_type_uuid();
}

Unique<IResourceDescription> RRGraphicsPipelineVK::deserialize_description(const DataValue& p_description, RenderAllocator* p_allocator) const {
    return typed_deserialize_description(p_description, p_allocator);
}

Ref<IRenderObject> RRGraphicsPipelineVK::compile_typed(
    IRenderGraph& /* p_graph */, std::string_view p_key, const RDParamsGraphicsPipelineVK& p_params) {
    RendererVK* renderer = acquire_renderer(p_key);
    RefRes<RUGraphicsPipeline> pipeline = load_source<RUGraphicsPipeline>(p_key, p_params.pipeline);
    const RenderContextVK* render_data = renderer->get_render_context();
    std::vector<VkFormat> color_formats;
    color_formats.reserve(p_params.color_formats.size());
    for (ImageFormatVK format : p_params.color_formats) {
        color_formats.push_back(to_vk_format(format));
    }
    GraphicsPipelineCompilerVKContext context{
        .allocator = render_data->allocator,
        .render_data = render_data,
        .render_cache = renderer->get_global_render_cache(),
        .renderer = renderer,
        .color_attachment_formats = std::move(color_formats),
        .depth_attachment_format = to_vk_format(p_params.depth_format),
        .stencil_attachment_format = to_vk_format(p_params.stencil_format),
        .polygon_mode = to_vk_polygon_mode(p_params.polygon_mode),
        .primitive_topology = to_vk_primitive_topology(p_params.primitive_topology),
    };
    // The pipeline is compiled for this registration's attachments, so it is cached, and named, under the registration key.
    return GraphicsPipelineCompilerVK(context).compile(p_key, pipeline).graphics_pipeline;
}

} // namespace WhiteBirdEngine
