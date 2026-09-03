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
#ifndef WBE_FILE_RT_RUN_STATIC_SET_VK_HH
#define WBE_FILE_RT_RUN_STATIC_SET_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/rt_run_static_triangle_set_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendererVK;

inline constexpr std::string_view RT_RUN_STATIC_SET = "run_static_set";

/**
 * @class RTRunStaticSetVK
 * @brief Minimal Vulkan bridge for explicit static-geometry set tasks.
 *
 * This bridge reuses the existing static-opaque attachment Config surface so callers can move to the explicit static-set
 * task API before the Vulkan geometry upload and draw path is implemented. Empty sets record a no-draw rendering pass
 * that still honors attachment load and clear operations. Recording a non-empty set currently throws.
 */
class RTRunStaticSetVK final : public RenderTaskVK {
public:
    /** @brief Reused attachment and material-contract configuration. */
    using Config = RTRunStaticTriangleSetVK::Config;
    /** @brief Reflected descriptor location within the compiled static-set pipeline. */
    using DescriptorBinding = RTRunStaticTriangleSetVK::DescriptorBinding;

    RTRunStaticSetVK() = delete;
    virtual ~RTRunStaticSetVK() override = default;
    RTRunStaticSetVK(const RTRunStaticSetVK&) = delete;
    RTRunStaticSetVK(RTRunStaticSetVK&&) noexcept = delete;
    RTRunStaticSetVK& operator=(const RTRunStaticSetVK&) = delete;
    RTRunStaticSetVK& operator=(RTRunStaticSetVK&&) noexcept = delete;

    /**
     * @brief Construct an explicit static-set task.
     *
     * The task retains the source static-geometry set and the selected graphics-pipeline resource for the future full
     * Vulkan implementation. The current bridge supports empty-set passes only.
     *
     * @param p_renderer Renderer providing Vulkan state.
     * @param p_set Static geometry set resource to render.
     * @param p_graphics_pipeline Graphics pipeline resource selected by the caller.
     * @param p_config Attachment and material-contract configuration.
     */
    RTRunStaticSetVK(
        RendererVK* p_renderer, RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_graphics_pipeline, const Config& p_config);

    virtual QueueFamilyVK queue_family() const override;
    virtual void validate_resources(const RendResViewVK& p_resources) const override;
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    virtual void record(VkCommandBuffer p_command_buffer, const RendResViewVK& p_resources, double p_delta) override;

    void write_descriptor_set(size_t p_set_index, const RODescriptorSetVK::Writer& p_writer);
    DescriptorBinding get_descriptor_binding(std::string_view p_field_path) const;

private:
    const BaseRenderDataVK* render_data = nullptr;
    RefRes<RUStaticGeometrySet> geometry_set = MEM_NULL;
    RefRes<RUGraphicsPipeline> graphics_pipeline = MEM_NULL;
    Config config;
    Unique<RTRunGraphicsPipelineIndirectVK> indirect_task = MEM_NULL;
    Ref<ROGraphicsPipelineVK> compiled_graphics_pipeline = MEM_NULL;
    GraphicsPipelineCompilerVK::DescriptorSetBindings descriptor_set_bindings;
};

} // namespace WhiteBirdEngine

#endif