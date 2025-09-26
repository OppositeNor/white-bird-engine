#include "function/render_vk/pipeline_layout_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

PipelineLayoutVK::PipelineLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data)
    : render_data(p_render_data) {
    (void)render_data;
}

PipelineLayoutVK::~PipelineLayoutVK() {

}

}
