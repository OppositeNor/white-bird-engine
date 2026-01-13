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
#ifndef WBE_FILE_RT_RUN_GRAPHICS_PIPELINE_VK_HH
#define WBE_FILE_RT_RUN_GRAPHICS_PIPELINE_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include <glm/glm.hpp>
#include <span>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RTRunGraphicsPipelineVK
 * @brief Render task that runs a graphics pipeline and produces an image.
 *
 */
struct RTRunGraphicsPipelineVK final : public RenderTaskVK {
    RTRunGraphicsPipelineVK() = delete;
    virtual ~RTRunGraphicsPipelineVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RTRunGraphicsPipelineVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_clear_color The clear color.
     * @param p_signal_in_flight_fence If set to true, the in_flight_fence will be signaled after the task is finished.
     * @param p_descriptor_sets The descriptor sets to be bound.
     */
    RTRunGraphicsPipelineVK(const BaseRenderDataVK* p_render_data, const glm::vec4& p_clear_color,
                            const std::span<const Ref<RODescriptorSetVK>>& p_descriptor_sets,
                            bool p_signal_in_flight_fence);

    virtual void init() override;

    virtual void perform(double p_delta) override;

    /**
     * @brief Add a descriptor set to the pipeline.
     * The set index is determined by the order of addition.
     *
     * @param p_descriptor_set The descriptor set to add.
     * @return *this.
     */
    RTRunGraphicsPipelineVK& add_descriptor_set(Ref<RODescriptorSetVK> p_descriptor_set) {
        descriptor_sets.push_back(p_descriptor_set);
        descriptor_sets_instances.push_back(p_descriptor_set->get_descriptor_set());
        return *this;
    }

private:
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkClearValue clear_value;
    bool signal_in_flight_fence = false;
    inline static std::vector<VkPipelineStageFlags2> wait_stages { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };
    void record_command_buffer_without_vertex_binding();
    void record_command_buffer_with_vertex_binding();
    Vector<Ref<RODescriptorSetVK>> descriptor_sets;
    // Keeping an easy access to the VkDescriptorSet instances.
    Vector<VkDescriptorSet> descriptor_sets_instances;
};

/**
 * @class RTRunGraphicsPipelineFactoryVK
 * @brief Factory for RTRunGraphicsPipeline.
 *
 */
class RTRunGraphicsPipelineFactoryVK final : public RenderTaskFactoryVK {
public:
    RTRunGraphicsPipelineFactoryVK() = delete;
    WBE_R6_NDC_DELETE_COPY_MOVE_OVERRIDE(RTRunGraphicsPipelineFactoryVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_signal_in_flight Should signal in flight fence.
     */
    RTRunGraphicsPipelineFactoryVK(const BaseRenderDataVK* p_render_data, bool p_signal_in_flight = false)
        : RenderTaskFactoryVK(p_render_data), signal_in_flight(p_signal_in_flight), clear_color(0.0F, 0.0F, 0.0F, 1.0F) {}

    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_clear_color The clear color.
     * @param p_signal_in_flight Should signal in flight fence.
     */
    RTRunGraphicsPipelineFactoryVK(const BaseRenderDataVK* p_render_data, const glm::vec4& p_clear_color, bool p_signal_in_flight = false)
        : RenderTaskFactoryVK(p_render_data), signal_in_flight(p_signal_in_flight), clear_color(p_clear_color) {}

    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_clear_color The clear color.
     * @param p_signal_in_flight Should signal in flight fence.
     * @param p_descriptor_sets The descriptor sets to be bound.
     */
    RTRunGraphicsPipelineFactoryVK(const BaseRenderDataVK* p_render_data, const glm::vec4& p_clear_color,
                                   const std::span<Ref<RODescriptorSetVK>>& p_descriptor_sets, bool p_signal_in_flight = false)
        : RenderTaskFactoryVK(p_render_data), signal_in_flight(p_signal_in_flight),
        clear_color(p_clear_color), descriptor_sets(p_descriptor_sets.begin(), p_descriptor_sets.end()) {}

    /**
     * @brief Constructor.
     *
     * @param p_render_data The render data.
     * @param p_signal_in_flight Should signal in flight fence.
     * @param p_descriptor_sets The descriptor sets to be bound.
     */
    RTRunGraphicsPipelineFactoryVK(const BaseRenderDataVK* p_render_data,
                                   const std::span<Ref<RODescriptorSetVK>>& p_descriptor_sets, bool p_signal_in_flight = false)
        : RenderTaskFactoryVK(p_render_data), signal_in_flight(p_signal_in_flight),
        clear_color(0.0F, 0.0F, 0.0F, 1.0F), descriptor_sets(p_descriptor_sets.begin(), p_descriptor_sets.end()) {}

    virtual Ref<RenderTask> create() const override {
        return make_ref<RTRunGraphicsPipelineVK>(render_data->allocator, render_data, clear_color, descriptor_sets, signal_in_flight);
    }
private:
    bool signal_in_flight;
    glm::vec4 clear_color;
    std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
};
} // namespace WhiteBirdEngine

#endif
