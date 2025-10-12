#include "function/render_vk/render_object_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderObjectVK::RenderObjectVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data) {
    init();
}

RenderObjectVK::~RenderObjectVK() {
    vkDestroySemaphore(render_data->logic_device, available_semaphore, render_data->allocation_callbacks);
}

void RenderObjectVK::init() {
    VkSemaphoreCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(render_data->logic_device, &create_info, render_data->allocation_callbacks, &available_semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore.");
    }
}

}
