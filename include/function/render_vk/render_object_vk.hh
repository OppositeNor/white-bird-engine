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
#ifndef WBE_FILE_RENDER_OBJECT_VK_HH
#define WBE_FILE_RENDER_OBJECT_VK_HH

#include "function/function_utils.hh"
#include "function/render/i_render_object.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class RenderObjectVK
 * @brief Vulkan render object.
 *
 */
class RenderObjectVK : public IRenderObject {
public:
    RenderObjectVK() = delete;
    virtual ~RenderObjectVK() override = default;
    RenderObjectVK(const RenderObjectVK&) = delete;
    RenderObjectVK(RenderObjectVK&&) noexcept = delete;
    RenderObjectVK& operator=(const RenderObjectVK&) = delete;
    RenderObjectVK& operator=(RenderObjectVK&&) noexcept = delete;

    /**
     * @brief Constructor.
     *
     * @param p_render_data The base render data.
     */
    RenderObjectVK(const BaseRenderDataVK* p_render_data) : render_data(p_render_data) {
        if (p_render_data == nullptr) {
            throw std::runtime_error("Render data is nullptr.");
        }
    }

protected:
    /**
     * @brief The base render data.
     */
    const BaseRenderDataVK* render_data;
    /**
     * @brief Allocate a device memory.
     *
     * @param p_memory_requirements The memroy requirements for allocating
     * memory.
     * @param p_domain The domain where this memory will be allocated.
     * @return The allocated device memory.
     */
    VkDeviceMemory allocate_device_memory(VkMemoryRequirements p_memory_requirements, DeviceDomain p_domain) const;
};

} // namespace WhiteBirdEngine

#endif
