#include "function/render_vk/render_object_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderObjectVK::RenderObjectVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data) {
    init();
}

RenderObjectVK::~RenderObjectVK() {
}

void RenderObjectVK::init() {
}

}
