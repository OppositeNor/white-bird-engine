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
#include "function/render/i_render_object.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <cstdint>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class GraphicsPipelineCompilerVK;
class ComputePipelineCompilerVK;

/**
 * @class ROPipelineVK
 * @brief Shared Vulkan pipeline runtime ownership.
 */
class ROPipelineVK : public IRenderObjectVK {
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
     * @brief Write one explicit uint2 descriptor index into push data.
     *
     * The retained resource keeps the object named by the descriptor alive.
     *
     * @param p_offset Reflected byte offset of the uint2.
     * @param p_index Descriptor heap index written into the x component.
     * @param p_resource Resource retained for the lifetime of the pipeline.
     */
    void set_descriptor_index(uint32_t p_offset, uint32_t p_index, Ref<IRenderObject> p_resource);

    /**
     * @brief Bind a resource-owned descriptor to a reflected push-data field.
     *
     * @param p_field_path Path relative to the shader push-data block.
     * @param p_resource Buffer, image view, or sampler owning the descriptor.
     */
    void set_descriptor_binding(std::string_view p_field_path, Ref<IRenderObject> p_resource);

    /**
     * @brief Bind a context-resolved resource container to a reflected field.
     */
    void set_descriptor_binding(std::string_view p_field_path, Ref<RenderObjectContainerVK> p_resource_container);

    /**
     * @brief Push the current descriptor-index block into a command buffer.
     * @param p_command_buffer Command buffer receiving the push data.
     */
    void push_descriptor_indices(VkCommandBuffer p_command_buffer);

protected:
    /**
     * @brief Construct shared runtime state.
     * @param p_render_data Base Vulkan render data.
     */
    explicit ROPipelineVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Publish the created Vulkan pipeline handle to the shared owner.
     * @param p_pipeline Created pipeline handle.
     */
    void set_pipeline(VkPipeline p_pipeline) {
        pipeline = p_pipeline;
    }

private:
    friend class PipelineCompilerVK;
    friend class GraphicsPipelineCompilerVK;

    void set_descriptor_layout(const ShaderDescriptorHeapLayoutVK& p_layout);
    void retain_descriptor_resource(Ref<IRenderObject> p_resource);

    VkPipeline pipeline = VK_NULL_HANDLE;
    Vector<uint8_t, RenderAllocator> descriptor_index_data;
    Vector<Ref<IRenderObject>, RenderAllocator> descriptor_resources;
    Vector<Ref<RenderObjectContainerVK>, RenderAllocator> descriptor_resource_containers;
    Vector<ShaderDescriptorHeapFieldVK, RenderAllocator> descriptor_fields;
};

} // namespace WhiteBirdEngine

#endif
