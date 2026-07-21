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
#ifndef WBE_FILE_RO_RENDER_PASS_VK_HH
#define WBE_FILE_RO_RENDER_PASS_VK_HH

#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <vector>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RORenderPassVK
 * @brief Render pass.
 *
 */
class WBE_META(WBE_RENDER_OBJECT) RORenderPassVK : public RenderObjectVK {
public:
    // Builder for RORenderPassVK
    struct Builder {
        friend RORenderPassVK;
        // Attachment descriptions.
        std::vector<VkAttachmentDescription> attachments;
        // Subpass descriptions.
        std::vector<VkSubpassDescription> subpasses;
        // Subpass dependencies.
        std::vector<VkSubpassDependency> dependencies;

        // Set attachment descriptions.
        Builder& set_attachments(const std::vector<VkAttachmentDescription>& p_attachments);
        // Add a single attachment description.
        Builder& add_attachment(const VkAttachmentDescription& p_attachment);
        // Set subpass descriptions.
        Builder& set_subpasses(const std::vector<VkSubpassDescription>& p_subpasses);
        // Add a single subpass description.
        Builder& add_subpass(const VkSubpassDescription& p_subpass);
        // Set subpass dependencies.
        Builder& set_dependencies(const std::vector<VkSubpassDependency>& p_dependencies);
        // Add a single subpass dependency.
        Builder& add_dependency(const VkSubpassDependency& p_dependency);

        Ref<RORenderPassVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<RORenderPassVK>(p_render_data->allocator, p_render_data, *this);
        }

        Unique<RORenderPassVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<RORenderPassVK>(p_render_data->allocator, p_render_data, *this);
        }

    private:
        VkRenderPass create_vk_object(const BaseRenderDataVK* p_render_data) const;
    };

struct AttachmentBuilder {
    // The format of the attachment.
    Required<VkFormat> format;
    // The sample rate per pixel.
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    // The load operation to the attachment.
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // The store operation to the attachment.
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
    // The load operation to the stencil buffer.
    VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // The store operation to the stencil buffer.
    VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // The initial layout of the image of the attachment.
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    // The final layout of the image of the attachment.
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_GENERAL;

    /**
     * @brief Create a builder for a color attachment.
     *
     * Default configuration:
     * - samples: VK_SAMPLE_COUNT_1_BIT
     * - loadOp: VK_ATTACHMENT_LOAD_OP_CLEAR
     * - storeOp: VK_ATTACHMENT_STORE_OP_STORE
     * - initialLayout: VK_IMAGE_LAYOUT_UNDEFINED
     * - finalLayout: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
     *
     * @param p_format The format of the color attachment.
     * @return The initialized builder.
     */
    static AttachmentBuilder color(VkFormat p_format) {
        return AttachmentBuilder{}
            .set_format(p_format)
            .set_samples(VK_SAMPLE_COUNT_1_BIT)
            .set_load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
            .set_initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .set_final_layout(VK_IMAGE_LAYOUT_GENERAL);
    }

    /**
     * @brief Create a builder for a depth/stencil attachment.
     *
     * Default configuration:
     * - samples: VK_SAMPLE_COUNT_1_BIT
     * - loadOp: VK_ATTACHMENT_LOAD_OP_CLEAR
     * - storeOp: VK_ATTACHMENT_STORE_OP_DONT_CARE
     * - stencilLoadOp: VK_ATTACHMENT_LOAD_OP_DONT_CARE
     * - stencilStoreOp: VK_ATTACHMENT_STORE_OP_DONT_CARE
     * - initialLayout: VK_IMAGE_LAYOUT_UNDEFINED
     * - finalLayout: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
     *
     * @param p_format The format of the depth/stencil attachment.
     * @return The initialized builder.
     */
    static AttachmentBuilder depth(VkFormat p_format) {
        return AttachmentBuilder{}
            .set_format(p_format)
            .set_samples(VK_SAMPLE_COUNT_1_BIT)
            .set_load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .set_store_op(VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .set_stencil_load_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE)
            .set_stencil_store_op(VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .set_initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .set_final_layout(VK_IMAGE_LAYOUT_GENERAL);
    }

    /**
     * @brief Create a builder for a resolve attachment.
     *
     * Default configuration:
     * - samples: VK_SAMPLE_COUNT_1_BIT
     * - loadOp: VK_ATTACHMENT_LOAD_OP_DONT_CARE
     * - storeOp: VK_ATTACHMENT_STORE_OP_STORE
     * - initialLayout: VK_IMAGE_LAYOUT_UNDEFINED
     * - finalLayout: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
     *
     * @param p_format The format of the resolve attachment.
     * @return The initialized builder.
     */
    static AttachmentBuilder resolve(VkFormat p_format) {
        return AttachmentBuilder{}
            .set_format(p_format)
            .set_samples(VK_SAMPLE_COUNT_1_BIT)
            .set_load_op(VK_ATTACHMENT_LOAD_OP_DONT_CARE)
            .set_store_op(VK_ATTACHMENT_STORE_OP_STORE)
            .set_initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .set_final_layout(VK_IMAGE_LAYOUT_GENERAL);
    }

    /**
     * @brief Create a builder for an input attachment.
     *
     * Default configuration:
     * - samples: VK_SAMPLE_COUNT_1_BIT
     * - loadOp: VK_ATTACHMENT_LOAD_OP_LOAD
     * - storeOp: VK_ATTACHMENT_STORE_OP_DONT_CARE
     * - initialLayout: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     * - finalLayout: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     *
     * @param p_format The format of the input attachment.
     * @return The initialized builder.
     */
    static AttachmentBuilder input(VkFormat p_format) {
        return AttachmentBuilder{}
            .set_format(p_format)
            .set_samples(VK_SAMPLE_COUNT_1_BIT)
            .set_load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .set_store_op(VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .set_initial_layout(VK_IMAGE_LAYOUT_GENERAL)
            .set_final_layout(VK_IMAGE_LAYOUT_GENERAL);
    }

    /**
     * @brief Set the format of the attachment.
     *
     * @param p_format The format of the attachment.
     * @return *this
     */
    AttachmentBuilder& set_format(VkFormat p_format) {
        format = p_format;
        return *this;
    }

    /**
     * @brief Set the sample count of the attachment.
     *
     * @param p_samples The sample count.
     * @return *this
     */
    AttachmentBuilder& set_samples(VkSampleCountFlagBits p_samples) {
        samples = p_samples;
        return *this;
    }

    /**
     * @brief Set the load operation.
     *
     * @param p_load_op The load operation.
     * @return *this
     */
    AttachmentBuilder& set_load_op(VkAttachmentLoadOp p_load_op) {
        load_op = p_load_op;
        return *this;
    }

    /**
     * @brief Set the store operation.
     *
     * @param p_store_op The store operation.
     * @return *this
     */
    AttachmentBuilder& set_store_op(VkAttachmentStoreOp p_store_op) {
        store_op = p_store_op;
        return *this;
    }

    /**
     * @brief Set the stencil load operation.
     *
     * @param p_load_op The stencil load operation.
     * @return *this
     */
    AttachmentBuilder& set_stencil_load_op(VkAttachmentLoadOp p_load_op) {
        stencil_load_op = p_load_op;
        return *this;
    }

    /**
     * @brief Set the stencil store operation.
     *
     * @param p_store_op The stencil store operation.
     * @return *this
     */
    AttachmentBuilder& set_stencil_store_op(VkAttachmentStoreOp p_store_op) {
        stencil_store_op = p_store_op;
        return *this;
    }

    /**
     * @brief Set the initial image layout.
     *
     * @param p_layout The initial image layout.
     * @return *this
     */
    AttachmentBuilder& set_initial_layout(VkImageLayout p_layout) {
        initial_layout = p_layout;
        return *this;
    }

    /**
     * @brief Set the final image layout.
     *
     * @param p_layout The final image layout.
     * @return *this
     */
    AttachmentBuilder& set_final_layout(VkImageLayout p_layout) {
        final_layout = p_layout;
        return *this;
    }

    VkAttachmentDescription build() const {
        return VkAttachmentDescription{
            .format = WBE_REQUIRED(format),
            .samples = samples,
            .loadOp = load_op,
            .storeOp = store_op,
            .stencilLoadOp = stencil_load_op,
            .stencilStoreOp = stencil_store_op,
            .initialLayout = initial_layout,
            .finalLayout = final_layout,
        };
    }
};

    RORenderPassVK() = delete;
    virtual ~RORenderPassVK() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(RORenderPassVK)

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     * @param p_builder Configuration builder.
     */
    RORenderPassVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the render pass instance.
     *
     * @return The render pass instance.
     */
    VkRenderPass get_render_pass() const {
        return render_pass;
    }

private:
    VkRenderPass render_pass = VK_NULL_HANDLE;

    void init(const Builder& p_builder);
};
} // namespace WhiteBirdEngine

#endif

