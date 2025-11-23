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
#ifndef __WBE_RO_BUFFER_VIEW_VK_HH__
#define __WBE_RO_BUFFER_VIEW_VK_HH__

#include "core/allocator/allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "ro_buffer.hh"
#include "utils/utils.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {
/**
 * @class ROBufferViewVK
 * @brief A buffer view.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) ROBufferViewVK final : public RenderObjectVK {
public:

    /**
     * @class Builder
     * @brief The builder to build ROBufferViewVK
     *
     */
    struct Builder {
        friend ROBufferViewVK;
        // The buffer this view is viewing on.
        Ref<ROBufferVK> buffer = MEM_NULL;
        // Buffer format.
        Required<VkFormat> format;
        // Buffer view read offset.
        VkDeviceSize offset = 0;
        // Buffer view read range.
        VkDeviceSize range = VK_WHOLE_SIZE;

        /**
         * @brief Set the buffer of this buffer view.
         *
         * @param p_buffer The buffer to set to.
         * @return *this.
         */
        Builder& set_buffer(Ref<ROBufferVK> p_buffer) {
            buffer = p_buffer;
            return *this;
        }

        /**
         * @brief Set the format of this buffer view.
         *
         * @param p_format The format of the buffer view.
         * @return *this.
         */
        Builder& set_format(VkFormat p_format) {
            format = p_format;
            return *this;
        }

        /**
         * @brief Set the offset of this buffer view.
         *
         * @param p_offset The offset of this buffer view.
         * @return *this.
         */
        Builder& set_offset(VkDeviceSize p_offset) {
            offset = p_offset;
            return *this;
        }

        /**
         * @brief Set the range of this buffer view.
         *
         * @param p_range The range of this buffer view.
         * @return *this.
         */
        Builder& set_range(VkDeviceSize p_range) {
            range = p_range;
            return *this;
        }
    private:
        VkBufferView create(const BaseRenderDataVK* p_render_data) const;
   };

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder The builder that builds this render object.
     */
    ROBufferViewVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROBufferViewVK() override;


    /**
     * @class ExposedFields
     * @brief The exposed fields of the buffer.
     *
     */
    struct ExposedFields {
        // The buffer this view is viewing on.
        Ref<ROBufferVK> buffer;
    };

    /**
     * @brief Get the Vulkan buffer view instance.
     *
     * @return The Vulkan buffer view instance.
     */
    VkBufferView get_buffer_view() const {
        return buffer_view;
    }

    /**
     * @brief Get the exposed fields.
     *
     * @return The exposed fields.
     */
    const ExposedFields& fields() const {
        return exposed_fields;
    }

    /**
     * @brief Get the exposed fields.
     *
     * @return The exposed fields.
     */
    ExposedFields& fields() {
        return exposed_fields;
    }

private:
    VkBufferView buffer_view;
    ExposedFields exposed_fields;
};

}

#endif
