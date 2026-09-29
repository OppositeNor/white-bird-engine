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
#ifndef WBE_FILE_PIPELINE_DESCRIPTOR_BINDINGS_VK_HH
#define WBE_FILE_PIPELINE_DESCRIPTOR_BINDINGS_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendResViewVK;

/**
 * @class PipelineDescriptorBindingsVK
 * @brief Resolution and push-data storage for one reflected pipeline descriptor layout.
 *
 * The object is neither copyable nor movable. It retains graph resource containers and resolves their current frame slots
 * while recording on the rendering thread.
 */
class PipelineDescriptorBindingsVK {
public:
    PipelineDescriptorBindingsVK() = delete;
    ~PipelineDescriptorBindingsVK() = default;
    PipelineDescriptorBindingsVK(const PipelineDescriptorBindingsVK&) = delete;
    PipelineDescriptorBindingsVK(PipelineDescriptorBindingsVK&&) noexcept = delete;
    PipelineDescriptorBindingsVK& operator=(const PipelineDescriptorBindingsVK&) = delete;
    PipelineDescriptorBindingsVK& operator=(PipelineDescriptorBindingsVK&&) noexcept = delete;

    /**
     * @brief Construct binding state for a reflected descriptor layout.
     * @param p_render_data Renderer state used for frame-slot resolution and push-data commands.
     * @param p_layout Immutable pipeline descriptor layout.
     */
    PipelineDescriptorBindingsVK(const BaseRenderDataVK* p_render_data, const ShaderDescriptorHeapLayoutVK& p_layout);

    /**
     * @brief Append the reflected shader accesses of every descriptor field.
     *
     * Accesses use the reflected resource identifiers, which the render graph resolves through the task's resource map.
     * Samplers are immutable and are not reported.
     *
     * @param p_reads Receives read accesses.
     * @param p_writes Receives write accesses.
     */
    void append_accesses(Vector<ResAccessVK, RenderAllocator>& p_reads, Vector<ResAccessVK, RenderAllocator>& p_writes) const;

    /**
     * @brief Resolve and retain every reflected logical resource from the graph resource view.
     * @param p_resources Graph-global resource view.
     */
    void bind_resources(const RendResViewVK& p_resources);

    /**
     * @brief Resolve the current frame slot and push descriptor indices and renderer default values for this pipeline.
     * @param p_command_buffer Command buffer receiving push data.
     */
    void push(VkCommandBuffer p_command_buffer);

private:
    const BaseRenderDataVK* render_data;
    ShaderDescriptorHeapLayoutVK layout;
    Vector<uint8_t, RenderAllocator> push_data;
    Vector<Ref<RenderObjectContainerVK>, RenderAllocator> containers;
};

} // namespace WhiteBirdEngine

#endif
