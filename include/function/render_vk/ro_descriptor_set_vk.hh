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

#include "core/allocator/allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "global/stl_allocator.hh"
#include "utils/utils.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

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
         * @brief Add a descriptor set layout.
         *
         * @param p_descriptor_set_layout The descriptor set layout to add.
         * @return Builder& Reference to this builder.
         */
        Builder& add_descriptor_set_layout(const Ref<RODescriptorSetLayoutVK>& p_descriptor_set_layout) {
            descriptor_set_layouts.push_back(p_descriptor_set_layout);
            return *this;
        }

    private:
        VkDescriptorSet build(const BaseRenderDataVK* p_render_data) const;
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
             * @brief The offset in the buffer.
             */
            VkDeviceSize offset = 0;
            /**
             * @brief The range of the buffer.
             */
            VkDeviceSize range = VK_WHOLE_SIZE;
        };

        /**
         * @brief The type of the descriptor.
         */
        enum class Type {
            // A buffer descriptor.
            BUFFER,
            // An image descriptor.
            IMAGE,
            // A texel buffer descriptor.
            TEXEL_BUFFER,
        };

        // TODO: Image info

        /**
         * @brief The buffer info to write.
         */
        std::vector<BufferInfo> buffer_infos;
        
        /**
         * @brief The binding to write to.
         */
        Required<uint32_t> binding;

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
         * @brief Add a buffer info.
         *
         * @param p_buffer_info The buffer info to add.
         * @return *this.
         */
        Writer& add_buffer_info(const BufferInfo& p_buffer_info) {
            buffer_infos.push_back(p_buffer_info);
            return *this;
        }
    };

    RODescriptorSetVK() = delete;
    virtual ~RODescriptorSetVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RODescriptorSetVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    RODescriptorSetVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Write to the descriptor set.
     *
     * @param p_writer The writer configuration.
     */
    void write_descriptor_set(const Writer& p_writer);

private:
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    Vector<Ref<RODescriptorSetLayoutVK>> descriptor_set_layouts;
    // Keep references to bound resources to prevent them from being destroyed.
    Map<uint32_t, Vector<Ref<RenderObjectVK>>> bound_objects;

    void prepare_binding(uint32_t p_binding);
};

} // namespace WhiteBirdEngine

#endif
