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
#include "function/render/i_render_object.hh"
#include "function/render/i_render_object_container.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include <cstdint>
#include <string>

namespace WhiteBirdEngine {

/**
 * @class RenderObjectContainerVK
 * @brief Abstract container that resolves the render object bound to one graph resource.
 */
class RenderObjectContainerVK : public IRenderObjectContainer {
public:
    virtual ~RenderObjectContainerVK() = default;
    RenderObjectContainerVK(const RenderObjectContainerVK&) = delete;
    RenderObjectContainerVK(RenderObjectContainerVK&&) noexcept = delete;
    RenderObjectContainerVK& operator=(const RenderObjectContainerVK&) = delete;
    RenderObjectContainerVK& operator=(RenderObjectContainerVK&&) noexcept = delete;

    virtual Ref<IRenderObject> get() override = 0;
    virtual Ref<IRenderObject> get() const override = 0;

    /**
     * @brief Resolve the resource for an explicit frame slot.
     * @param p_frame_slot Frame slot to resolve.
     * @return Resolved render object.
     */
    virtual Ref<IRenderObject> get(uint32_t p_frame_slot) override = 0;

    /**
     * @brief Resolve the resource for an explicit frame slot.
     * @param p_frame_slot Frame slot to resolve.
     * @return Resolved render object.
     */
    virtual Ref<IRenderObject> get(uint32_t p_frame_slot) const override = 0;

    /**
     * @brief Replace the resource bound to one explicit slot.
     *
     * @param p_frame_slot Slot index to update.
     * @param p_resource Replacement resource.
     */
    virtual void set(uint32_t p_frame_slot, Ref<IRenderObjectVK> p_resource) = 0;

    /**
     * @brief Clear the resource bound to one explicit slot.
     *
     * @param p_frame_slot Slot index to clear.
     */
    virtual void clear(uint32_t p_frame_slot) = 0;

    /**
     * @brief Clear all slot bindings retained by this container.
     */
    virtual void clear() = 0;

    /**
     * @brief Return structured descriptor-debug rows owned by this container.
     *
     * Implementations must inspect only already-bound resources and must not allocate descriptors or synchronize per-frame state.
     *
     * @return Flattened descriptor-debug rows.
     */
    virtual DebugDataRows descriptor_debug_data() const = 0;

    /**
     * @brief Serialize the descriptor-debug rows into one compact JSON array.
     *
     * @return Compact JSON array string.
     */
    std::string to_string() const;

protected:
    RenderObjectContainerVK() = default;
};

} // namespace WhiteBirdEngine

#endif
