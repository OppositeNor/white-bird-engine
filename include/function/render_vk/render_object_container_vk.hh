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
#ifndef WBE_FILE_RENDER_OBJECT_CONTAINER_VK_HH
#define WBE_FILE_RENDER_OBJECT_CONTAINER_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
#include "function/render_vk/render_object_vk.hh"
#include <cstdint>

namespace WhiteBirdEngine {

/**
 * @class RenderObjectContainerVK
 * @brief Abstract container that resolves the render object bound to one graph resource.
 */
class RenderObjectContainerVK {
public:
    virtual ~RenderObjectContainerVK() = default;
    RenderObjectContainerVK(const RenderObjectContainerVK&) = delete;
    RenderObjectContainerVK(RenderObjectContainerVK&&) noexcept = delete;
    RenderObjectContainerVK& operator=(const RenderObjectContainerVK&) = delete;
    RenderObjectContainerVK& operator=(RenderObjectContainerVK&&) noexcept = delete;

    virtual Ref<RenderObjectVK> get() = 0;
    virtual Ref<RenderObjectVK> get() const = 0;

    /**
     * @brief Resolve the resource for an explicit frame slot.
     * @param p_frame_slot Frame slot to resolve.
     * @return Resolved render object.
     */
    virtual Ref<RenderObjectVK> get(uint32_t p_frame_slot) = 0;

    /**
     * @brief Resolve the resource for an explicit frame slot.
     * @param p_frame_slot Frame slot to resolve.
     * @return Resolved render object.
     */
    virtual Ref<RenderObjectVK> get(uint32_t p_frame_slot) const = 0;

    RendResTID get_resource_type_id() const;

protected:
    RenderObjectContainerVK() = default;

    void retain_resource_type(Ref<RenderObjectVK> p_resource);

    RendResTID resource_type_id = 0;
};

} // namespace WhiteBirdEngine

#endif
