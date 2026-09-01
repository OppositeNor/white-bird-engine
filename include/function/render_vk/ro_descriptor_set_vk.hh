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
#ifndef WBE_RO_DESCRIPTOR_SET_HH
#define WBE_RO_DESCRIPTOR_SET_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class ROPipelineVK;

/**
 * @class RODescriptorSetVK
 * @brief Descriptor set
 *
 */
class WBE_META(WBE_RENDER_OBJECT) RODescriptorSetVK final : public RenderObjectVK {
public:
    /**
     * @class Builder
     * @brief The builder for RODescriptorSetVK.
     *
     */
    struct Builder {
        friend RODescriptorSetVK;
        /**
         * @brief The descriptor set layouts.
         */
        std::vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts;
        /**
         * @brief Actual descriptor count for the variable-count binding, or zero when the layout has no such binding.
         */
        uint32_t variable_descriptor_count = 0;

        /**
         * @brief Add a descriptor set layout.
         *
         * @param p_descriptor_set_layout The descriptor set layout to add.
         * @return Builder& Reference to this builder.
         */
        Builder& add_descriptor_set_layout(const Ref<RODescriptorSetLayoutVK>& p_descriptor_set_layout) {
            descriptor_set_layouts.push_back(p_descriptor_set_layout);
            return *this;
        }

        /**
         * @brief Set the actual descriptor count allocated for the layout's variable-count binding.
         *
         * @param p_descriptor_count The non-zero descriptor count to allocate.
         * @return *this.
         */
        Builder& set_variable_descriptor_count(uint32_t p_descriptor_count) {
            variable_descriptor_count = p_descriptor_count;
            return *this;
        }

        Ref<RODescriptorSetVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<RODescriptorSetVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<RODescriptorSetVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<RODescriptorSetVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkDescriptorSet create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @class Writer
     * @brief Descriptor set writer.
     *
     */
    struct Writer {
        /**
         * @class BufferInfo
         * @brief Information about a buffer to write to the descriptor set.
         *
         */
        struct BufferInfo {
            /**
             * @brief The buffer instance.
             */
            Ref<ROBufferVK> buffer = MEM_NULL;
            /**
             * @brief Frame-resolved buffer container, used when buffer is null.
             */
            Ref<RenderObjectContainerVK> buffer_container = MEM_NULL;
            /**
             * @brief The offset in the buffer.
             */
            VkDeviceSize offset = 0;
            /**
             * @brief The range of the buffer.
             */
            VkDeviceSize range = VK_WHOLE_SIZE;
        };

        /**
         * @class ImageInfo
         * @brief Image and sampler state for image-family descriptor writes.
         *
         * SAMPLED_IMAGE writes require image_view, SAMPLER writes require
         * sampler, and COMBINED_IMAGE_SAMPLER writes require both.
         */
        struct ImageInfo {
            /**
             * @brief Image view to bind for sampled-image descriptors.
             */
            Ref<ROImageViewVK> image_view = MEM_NULL;
            /**
             * @brief Frame-resolved image-view container, used when image_view is null.
             */
            Ref<RenderObjectContainerVK> image_view_container = MEM_NULL;
            /**
             * @brief Sampler to bind for sampler descriptors.
             */
            Ref<ROSamplerVK> sampler = MEM_NULL;
            /**
             * @brief Frame-resolved sampler container, used when sampler is null.
             */
            Ref<RenderObjectContainerVK> sampler_container = MEM_NULL;
            /**
             * @brief Image layout visible to the shader descriptor.
             */
            VkImageLayout image_layout = VK_IMAGE_LAYOUT_GENERAL;
        };

        /**
         * @brief The storage buffer info to write.
         */
        std::vector<BufferInfo> storage_buffer_infos;

        /**
         * @brief The uniform buffer info to write.
         */
        std::vector<BufferInfo> uniform_buffer_infos;

        /**
         * @brief Sampled images to write to the binding.
         */
        std::vector<ImageInfo> sampled_image_infos;

        /**
         * @brief Storage images to write to the binding.
         */
        std::vector<ImageInfo> storage_image_infos;

        /**
         * @brief Samplers to write to the binding.
         */
        std::vector<ImageInfo> sampler_infos;

        /**
         * @brief Combined image samplers to write to the binding.
         */
        std::vector<ImageInfo> combined_image_sampler_infos;

        /**
         * @brief The binding to write to.
         */
        Required<uint32_t> binding;
        /**
         * @brief First array element to write within the binding.
         */
        uint32_t dst_array_element = 0;

        /**
         * @brief Set the binding.
         *
         * @param p_binding The binding to set.
         * @return *this.
         */
        Writer& set_binding(uint32_t p_binding) {
            binding = p_binding;
            return *this;
        }

        /**
         * @brief Set the first array element to write.
         *
         * @param p_dst_array_element The first array element to write.
         * @return *this.
         */
        Writer& set_dst_array_element(uint32_t p_dst_array_element) {
            dst_array_element = p_dst_array_element;
            return *this;
        }

        /**
         * @brief Add a storage_buffer info.
         *
         * @param p_buffer_info The buffer info to add.
         * @return *this.
         */
        Writer& add_storage_buffer_info(const BufferInfo& p_buffer_info) {
            storage_buffer_infos.push_back(p_buffer_info);
            return *this;
        }

        /**
         * @brief Add a uniform_buffer info.
         *
         * @param p_buffer_info The buffer info to add.
         * @return *this.
         */
        Writer& add_uniform_buffer_info(const BufferInfo& p_buffer_info) {
            uniform_buffer_infos.push_back(p_buffer_info);
            return *this;
        }

        /**
         * @brief Add a uniform_buffer info.
         *
         * @param p_buffer The buffer instance.
         * @param p_offset The offset in the buffer.
         * @param p_range The range of the buffer.
         * @return *this.
         */
        Writer& add_uniform_buffer_info(Ref<ROBufferVK> p_buffer, VkDeviceSize p_offset, VkDeviceSize p_range) {
            uniform_buffer_infos.push_back({.buffer = p_buffer, .offset = p_offset, .range = p_range});
            return *this;
        }

        /**
         * @brief Add frame-resolved uniform-buffer information.
         * @param p_buffer_container Container resolving one buffer for each frame slot.
         * @param p_offset Offset in the resolved buffer.
         * @param p_range Range in the resolved buffer.
         * @return *this.
         */
        Writer& add_uniform_buffer_info(Ref<RenderObjectContainerVK> p_buffer_container, VkDeviceSize p_offset, VkDeviceSize p_range) {
            uniform_buffer_infos.push_back(
                {.buffer = MEM_NULL, .buffer_container = std::move(p_buffer_container), .offset = p_offset, .range = p_range});
            return *this;
        }

        /**
         * @brief Return whether any descriptor resource must be resolved for a frame slot.
         * @return True when the writer contains a render-object container.
         */
        bool has_contextual_resources() const;

        /**
         * @brief Resolve every contextual resource for one frame slot.
         * @param p_frame_slot Frame slot whose resources are required.
         * @return Writer containing only concrete render objects.
         */
        Writer resolve(uint32_t p_frame_slot) const;

        /**
         * @brief Add sampled image information.
         *
         * @param p_image_info The image info to add.
         * @return *this.
         */
        Writer& add_sampled_image_info(const ImageInfo& p_image_info) {
            sampled_image_infos.push_back(p_image_info);
            return *this;
        }

        /**
         * @brief Add storage image information.
         *
         * @param p_image_info The image info to add.
         * @return *this.
         */
        Writer& add_storage_image_info(const ImageInfo& p_image_info) {
            storage_image_infos.push_back(p_image_info);
            return *this;
        }

        /**
         * @brief Add sampler information.
         *
         * @param p_image_info The sampler info to add.
         * @return *this.
         */
        Writer& add_sampler_info(const ImageInfo& p_image_info) {
            sampler_infos.push_back(p_image_info);
            return *this;
        }

        /**
         * @brief Add combined image sampler information.
         *
         * @param p_image_info The combined image sampler info to add.
         * @return *this.
         */
        Writer& add_combined_image_sampler_info(const ImageInfo& p_image_info) {
            combined_image_sampler_infos.push_back(p_image_info);
            return *this;
        }
    };

    RODescriptorSetVK() = delete;
    virtual ~RODescriptorSetVK() override;
    RODescriptorSetVK(const RODescriptorSetVK&) = delete;
    RODescriptorSetVK(RODescriptorSetVK&&) noexcept = delete;
    RODescriptorSetVK& operator=(const RODescriptorSetVK&) = delete;
    RODescriptorSetVK& operator=(RODescriptorSetVK&&) noexcept = delete;

    TypeID get_type_uuid() const override;

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    RODescriptorSetVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Write a descriptor generation.
     *
     * Before the set is first acquired this initializes it in place. Once acquired, the write creates and publishes a copy so
     * command buffers using an older generation remain valid. Calls must be externally synchronized with command recording.
     *
     * @param p_writer The writer configuration.
     */
    void write_descriptor_set(const Writer& p_writer);

    /**
     * @brief Acquire the current descriptor-set generation for command recording.
     *
     * @return The VkDescriptorSet instance.
     */
    VkDescriptorSet get_descriptor_set() const {
        has_been_acquired = true;
        return descriptor_set;
    }

private:
    friend ROPipelineVK;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    uint32_t variable_descriptor_count = 0;
    mutable bool has_been_acquired = false;
    Vector<Ref<RODescriptorSetLayoutVK>, RenderAllocator> descriptor_set_layouts;
    // Keep references per binding and array element to prevent bound resources from being destroyed.
    Map<uint64_t, Vector<Ref<RenderObjectVK>, RenderAllocator>, RenderAllocator> bound_objects;

    void validate_writer(const Writer& p_writer) const;
    VkDescriptorSet create_descriptor_set() const;
    VkDescriptorSet create_descriptor_set_for_write() const;
    void publish_descriptor_set(VkDescriptorSet p_descriptor_set);
    void retire_descriptor_set(VkDescriptorSet p_descriptor_set) const;
    Ref<RODescriptorSetVK> create_copy() const;
};

} // namespace WhiteBirdEngine

#endif
