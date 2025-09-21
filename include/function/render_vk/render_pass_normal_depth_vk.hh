#ifndef __WBE_RENDER_PASS_NORMAL_DEPTH_VK_HH__
#define __WBE_RENDER_PASS_NORMAL_DEPTH_VK_HH__

#include "core/reflection/reflection_defs.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_pass.hh"
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

class RenderPassNormalDepthVK final : public RenderPass {
public:
    WBE_RENDER_PASS(RenderPassNormalDepthVK, normal_depth, VULKAN);


    WBE_STRUCT(RenderPassNormalDepthVKInput, WBE_RENDER_PASS_DATA) : public RenderData {
        VkDevice logic_device;
        VkAllocationCallbacks* allocation_callbacks;
    };

    RenderPassNormalDepthVK() = default;
    virtual ~RenderPassNormalDepthVK() override;
    RenderPassNormalDepthVK(const RenderPassNormalDepthVK&) = delete;
    RenderPassNormalDepthVK(RenderPassNormalDepthVK&&) = delete;
    RenderPassNormalDepthVK& operator=(const RenderPassNormalDepthVK&) = delete;
    RenderPassNormalDepthVK& operator=(RenderPassNormalDepthVK&&) = delete;

    virtual Ref<RenderData> setup(Ref<RenderData> p_input) override;

    virtual Ref<RenderData> involke(Ref<RenderData> p_input) override;


private:
    bool initialized = false;
    VkRenderPass render_pass;

    VkAttachmentDescription create_result_color_attachment();
};

}

#endif
