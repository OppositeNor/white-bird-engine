#ifndef __WBE_RENDER_TASK_VK_HH__
#define __WBE_RENDER_TASK_VK_HH__

#include "function/render/render_task.hh"
namespace WhiteBirdEngine {

struct RenderTaskVK : public RenderTask {
    RenderTaskVK() = default;
    virtual ~RenderTaskVK() {}
    /**
     * @brief The semaphores of the required resources. This will be filled in the order
     * of the req_res field.
     */
    std::vector<VkSemaphore> req_res_semaphores;
    /**
     * @brief The semaphores of the produced resources. This will be filled in the order
     * of the prod_res field.
     */
    std::vector<VkSemaphore> prod_res_semaphores;
};

}

#endif
