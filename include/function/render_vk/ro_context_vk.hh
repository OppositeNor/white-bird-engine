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
#ifndef WBE_FILE_RO_CONTEXT_VK_HH
#define WBE_FILE_RO_CONTEXT_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <cstdint>
#include <functional>

namespace WhiteBirdEngine {

/**
 * @class ROContextVK
 * @brief Per-frame render-object wrapper for Vulkan resources that cannot be shared safely across frames in flight.
 *
 * The wrapper stores one render object per frame slot and returns either the
 * current-frame instance or an explicitly indexed instance.
 */
class WBE_META(WBE_RENDER_OBJECT) ROContextVK final : public RenderObjectVK {
public:
    /** Factory used to construct one render object per frame slot. */
    using Factory = std::function<Ref<RenderObjectVK>(uint32_t p_context_id)>;

    /**
     * @brief Construct one render-object instance for every frame in flight.
     *
     * @param p_render_data Base Vulkan render data that owns the frame-slot state.
     * @param p_resource_type_id Type UUID of the wrapped render-object type.
     * @param p_factory Factory invoked once per frame slot.
     */
    ROContextVK(const BaseRenderDataVK* p_render_data, RendResTID p_resource_type_id, Factory p_factory);
    virtual ~ROContextVK() override = default;
    ROContextVK(const ROContextVK&) = delete;
    ROContextVK(ROContextVK&&) noexcept = delete;
    ROContextVK& operator=(const ROContextVK&) = delete;
    ROContextVK& operator=(ROContextVK&&) noexcept = delete;

    /**
     * @brief Get the wrapped resource type id shared by all frame-slot instances.
     */
    RendResTID get_resource_type_id() const;

    /**
     * @brief Get the number of frame-slot instances owned by this wrapper.
     */
    uint32_t get_context_count() const;

    /**
     * @brief Get the resource for the current frame slot.
     */
    Ref<RenderObjectVK> get() const;

    /**
     * @brief Get the resource for one specific frame slot.
     *
     * @param p_context_id Frame-slot index.
     */
    Ref<RenderObjectVK> get(uint32_t p_context_id) const;

private:
    RendResTID resource_type_id = 0;
    Vector<Ref<RenderObjectVK>, RenderAllocator> resources;
};

} // namespace WhiteBirdEngine

#endif