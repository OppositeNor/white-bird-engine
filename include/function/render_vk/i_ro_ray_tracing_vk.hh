#ifndef WBE_FILE_RO_RAY_TRACING_VK
#define WBE_FILE_RO_RAY_TRACING_VK

#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
namespace WhiteBirdEngine {
class IRORayTracingVK : public IRenderObjectVK {
public:
    IRORayTracingVK() = delete;
    virtual ~IRORayTracingVK() override = default;
    IRORayTracingVK(const IRORayTracingVK&) = delete;
    IRORayTracingVK(IRORayTracingVK&&) noexcept = delete;
    IRORayTracingVK& operator=(const IRORayTracingVK&) = delete;
    IRORayTracingVK& operator=(IRORayTracingVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     */
    IRORayTracingVK(const BaseRenderDataVK* p_render_data, const BaseRayTracingDataVK* p_ray_tracing_data)
        : IRenderObjectVK(p_render_data), ray_tracing_data(p_ray_tracing_data) {
        if (p_ray_tracing_data == nullptr) {
            throw std::runtime_error("Ray tracing data is nullptr.");
        }
    }

protected:
    const BaseRayTracingDataVK* ray_tracing_data;
};

} // namespace WhiteBirdEngine

#endif
