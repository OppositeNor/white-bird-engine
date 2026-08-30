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
#ifndef WBE_FILE_RO_PIPELINE_VK_HH
#define WBE_FILE_RO_PIPELINE_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class GraphicsPipelineCompilerVK;
class ComputePipelineCompilerVK;

/**
 * @class ROPipelineVK
 * @brief Shared Vulkan pipeline runtime ownership and descriptor binding support.
 *
 * The pipeline retains its layout and one descriptor-set instance per frame slot. Writes publish versioned descriptor state;
 * binding synchronizes the selected, fence-safe frame slot before command recording. Descriptor publication retains
 * RODescriptorSetVK copy-on-write and delayed-destruction behavior.
 */
class ROPipelineVK : public RenderObjectVK {
public:
    ROPipelineVK() = delete;
    virtual ~ROPipelineVK() override;
    ROPipelineVK(const ROPipelineVK&) = delete;
    ROPipelineVK(ROPipelineVK&&) noexcept = delete;
    ROPipelineVK& operator=(const ROPipelineVK&) = delete;
    ROPipelineVK& operator=(ROPipelineVK&&) noexcept = delete;

    /**
     * @brief Return the owned Vulkan pipeline handle.
     * @return Pipeline handle.
     */
    VkPipeline get_pipeline() const {
        return pipeline;
    }

    /**
     * @brief Return the retained Vulkan pipeline layout.
     * @return Pipeline layout handle.
     */
    VkPipelineLayout get_pipeline_layout() const;

    /**
     * @brief Publish the latest state for one pipeline-owned descriptor binding.
     *
     * The state is applied lazily to each frame-slot descriptor set when that slot is next bound. Writers containing render-object
     * containers resolve those containers for the selected frame slot. Calls must be externally synchronized with descriptor
     * binding; logic-thread changes should publish through a render-thread job.
     * @param p_set_index Descriptor-set index in pipeline layout order.
     * @param p_writer Descriptor update preserving copy-on-write behavior.
     * @throws std::runtime_error When the set index is invalid.
     */
    void write_descriptor_set(uint32_t p_set_index, const RODescriptorSetVK::Writer& p_writer);

    /**
     * @brief Bind every pipeline-owned descriptor set in layout order.
     * @param p_command_buffer Command buffer receiving the binding.
     * @param p_bind_point Graphics or compute pipeline bind point.
     */
    void bind_descriptor_sets(VkCommandBuffer p_command_buffer, VkPipelineBindPoint p_bind_point);

protected:
    /**
     * @brief Construct shared runtime state and allocate default descriptor sets from the layout.
     * @param p_render_data Base Vulkan render data.
     * @param p_pipeline_layout Retained pipeline layout.
     */
    ROPipelineVK(const BaseRenderDataVK* p_render_data, Ref<ROPipelineLayoutVK> p_pipeline_layout);

    /**
     * @brief Publish the created Vulkan pipeline handle to the shared owner.
     * @param p_pipeline Created pipeline handle.
     */
    void set_pipeline(VkPipeline p_pipeline) {
        pipeline = p_pipeline;
    }

    Ref<ROPipelineLayoutVK> pipeline_layout;

private:
    friend GraphicsPipelineCompilerVK;
    friend ComputePipelineCompilerVK;

    void set_descriptor_sets(std::vector<Ref<RODescriptorSetVK>> p_descriptor_sets);
    Ref<RODescriptorSetVK> get_descriptor_set(uint32_t p_frame_slot, uint32_t p_set_index) const;
    void synchronize_descriptor_sets(uint32_t p_frame_slot);

    struct DescriptorWrite {
        uint32_t set_index = 0;
        uint32_t binding = 0;
        uint32_t dst_array_element = 0;
        uint64_t generation = 0;
        RODescriptorSetVK::Writer writer;
        Vector<uint64_t, RenderAllocator> applied_generations;

        explicit DescriptorWrite(RenderAllocator* p_allocator) : applied_generations(p_allocator) {
        }
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t descriptor_set_count = 0;
    uint64_t descriptor_generation = 0;
    Vector<Ref<RODescriptorSetVK>, RenderAllocator> descriptor_sets;
    Vector<DescriptorWrite, RenderAllocator> descriptor_writes;
    Vector<VkDescriptorSet, RenderAllocator> descriptor_set_instances;
};

} // namespace WhiteBirdEngine

#endif
