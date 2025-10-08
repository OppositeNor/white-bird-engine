#include "function/render_vk/render_task_vk.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

RenderTaskVK::RenderTaskVK(const BaseRenderDataVK* p_render_data)
    : render_data(p_render_data) {
    if (p_render_data == nullptr) {
        throw std::runtime_error("Render data is nullptr.");
    }
    init();
}

RenderTaskVK::~RenderTaskVK() {
    vkDestroySemaphore(render_data->logic_device, semaphore, render_data->allocation_callbacks);
}

void RenderTaskVK::init() {
    VkSemaphoreCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(render_data->logic_device, &create_info, render_data->allocation_callbacks, &semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore.");
    }
}

}

