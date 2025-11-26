#ifndef __WBE_RO_BUFFER_VK_HH__
#define __WBE_RO_BUFFER_VK_HH__

#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_utils.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <set>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class WBE_META(WBE_RENDER_OBJECT) ROBufferVK final : public RenderObjectVK {
public:
    /**
     * @class Builder
     * @brief The builder object for ROBufferVK.
     *
     */
    struct Builder {
        friend ROBufferVK;

        // The size of the buffer.
        size_t size = WBE_KiB(4);
        // The usage of the buffer.
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        // Queue families this buffer is going to be used across. Either this size is 1 or is empty will
        // make the buffer created in exclusive sharing mode, otherwise it will be created in concurrent
        // sharing mode.
        std::set<uint32_t> queue_family_indices;

        /**
         * @brief Set the size of the buffer.
         *
         * @param p_size The size to be set to.
         * @return *this.
         */
        Builder& set_size(size_t p_size) {
            size = p_size;
            return *this;
        }

        /**
         * @brief Set the usage of this buffer.
         *
         * @param p_usage The usage of this buffer.
         * @return *this.
         */
        Builder& set_usage(VkBufferUsageFlags p_usage) {
            usage = p_usage;
            return *this;
        }

        /**
         * @brief Add usage to this buffer.
         *
         * @param p_usage The usage to be added to this buffer.
         * @return *this.
         */
        Builder& add_usage(VkBufferUsageFlags p_usage) {
            usage |= p_usage;
            return *this;
        }

        /**
         * @brief Set the queue families that may use this buffer.
         *
         * @param p_queue_family_indices The queue family indices that might use this buffer.
         * @return *this.
         */
        Builder& set_queue_family_indices(const std::set<uint32_t>& p_queue_family_indices) {
            queue_family_indices = p_queue_family_indices;
            return *this;
        }

        /**
         * @brief Set the queue families that may use this buffer.
         *
         * @param p_queue_family_indices The queue family indices that might use this buffer.
         * @return *this.
         */
        Builder& set_queue_family_indices(std::set<uint32_t>&& p_queue_family_indices) {
            queue_family_indices = std::move(p_queue_family_indices);
            return *this;
        }
    private:
        VkBuffer build(const BaseRenderDataVK* p_render_data) const;
    };

    /**
     * @brief Constructor.
     *
     * @param p_builder The builder instance.
     */
    ROBufferVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);
    virtual ~ROBufferVK() override;

    /**
     * @brief Get the buffer instance.
     *
     * @return The buffer instance.
     */
    VkBuffer get_buffer() const {
        return buffer;
    }

private:
    VkBuffer buffer;
};
}

#endif
