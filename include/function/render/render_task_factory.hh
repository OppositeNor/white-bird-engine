#ifndef __WBE_RENDER_TASK_FACTORY_HH__
#define __WBE_RENDER_TASK_FACTORY_HH__

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
namespace WhiteBirdEngine {

/**
 * @brief Render task factory.
 *
 * @tparam Child The child class.
 */
template <typename Child>
class RenderTaskFactory {
public:
    template <typename... Args>
    Ref<RenderTask> create(const char* p_task_name, Args&& ...p_args);
};

}


#endif
