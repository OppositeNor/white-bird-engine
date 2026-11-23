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
#ifndef WBE_FILE_I_REND_OBJECT_CONTAINER_HH
#define WBE_FILE_I_REND_OBJECT_CONTAINER_HH

#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/debug_data.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/i_rend_task.hh"
#include "function/render/rend_defs.hh"
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {

/**
 * @class IRendObjectContainer
 * @brief Stable resource binding that resolves one render object per slot at use time. Neither copyable nor movable.
 *
 * Every container creates its render objects with a factory, one call per slot, when it is constructed and again when
 * recreate() is called. All resources of a container have the same concrete type. Confined to the rendering thread, like
 * the render objects it holds.
 */
class IRendObjectContainer {
public:
    /**
     * @brief Factory of the render object of one slot.
     *
     * Called with every slot index when the container is constructed and when it is recreated. It must return a non-null
     * render object, and is called on the thread that constructs or recreates the container.
     */
    using Factory = std::function<Ref<IRendObject>(uint32_t)>;

    IRendObjectContainer() = default;
    virtual ~IRendObjectContainer() = default;
    IRendObjectContainer(const IRendObjectContainer&) = delete;
    IRendObjectContainer(IRendObjectContainer&&) noexcept = delete;
    IRendObjectContainer& operator=(const IRendObjectContainer&) = delete;
    IRendObjectContainer& operator=(IRendObjectContainer&&) noexcept = delete;

    /**
     * @brief Resolve the resource of the first slot.
     * @return Resolved render object.
     */
    virtual Ref<IRendObject> get() = 0;

    /**
     * @brief Resolve the resource of the first slot.
     * @return Resolved render object.
     */
    virtual Ref<IRendObject> get() const = 0;

    /**
     * @brief Resolve the resource for an explicit slot.
     * @param p_slot Slot in the container.
     * @return Resolved render object.
     */
    virtual Ref<IRendObject> get(uint32_t p_slot) = 0;

    /**
     * @brief Resolve the resource for an explicit slot.
     * @param p_slot Slot in the container.
     * @return Resolved render object.
     */
    virtual Ref<IRendObject> get(uint32_t p_slot) const = 0;

    /**
     * @brief Replace the resource bound to one explicit slot.
     *
     * @param p_slot Slot index to update.
     * @param p_resource Replacement resource.
     * @throws std::runtime_error If the slot is out of range or the resource's concrete type differs from the container's.
     */
    virtual void set(uint32_t p_slot, Ref<IRendObject> p_resource) = 0;

    /**
     * @brief Clear the resource bound to one explicit slot.
     *
     * @param p_slot Slot index to clear.
     * @throws std::runtime_error If the slot is out of range.
     */
    virtual void clear(uint32_t p_slot) = 0;

    /**
     * @brief Clear all slot bindings retained by this container.
     */
    virtual void clear() = 0;

    /**
     * @brief Replace the render object of every slot with a new one from the container's factory.
     *
     * The previous render objects are released; users that resolve the container observe the new objects. Must be called on
     * the rendering thread while no submitted work uses the previous objects.
     *
     * @throws std::runtime_error If the factory returns null for a slot.
     */
    virtual void recreate() = 0;

    /**
     * @brief Get the number of slots.
     * @return Slot count; 1 for a container that resolves the same object for every slot.
     */
    virtual uint32_t get_slot_count() const = 0;

    /**
     * @brief Return the descriptor-debug rows of the container's resources, flattened.
     *
     * Implementations must inspect only already-bound resources and must not allocate descriptors or synchronize per-slot state.
     *
     * @param p_alloc The allocator of the returned rows.
     * @return Flattened descriptor-debug rows.
     */
    virtual DebugDataRows desc_debug_data(RendAlloc* p_alloc) const = 0;

    /**
     * @brief Serialize the descriptor-debug rows into one compact JSON array.
     *
     * @param p_alloc The allocator of the intermediate rows.
     * @return Compact JSON array string.
     */
    std::string to_string(RendAlloc* p_alloc) const;

    /**
     * @brief Get the concrete type of the container's resources.
     * @return The type UUID, or 0 before any resource was stored.
     */
    RendResTID get_resource_type_id() const {
        return resource_type_id;
    }

protected:
    /**
     * @brief Record the concrete type of a stored resource.
     *
     * @param p_resource The stored resource; null is ignored.
     * @throws std::runtime_error If the resource's concrete type differs from the one already recorded.
     */
    void retain_resource_type(const Ref<IRendObject>& p_resource) {
        if (p_resource == MEM_NULL) {
            return;
        }
        RendResTID concrete_type_id = p_resource->get_type_uuid();
        if (resource_type_id == 0) {
            resource_type_id = concrete_type_id;
            return;
        }
        if (resource_type_id != concrete_type_id) {
            throw std::runtime_error("Render object container resources must have the same concrete type.");
        }
    }

    /**
     * @brief The concrete type of the container's resources, or 0 before any resource was stored.
     */
    RendResTID resource_type_id = 0;
};

} // namespace WhiteBirdEngine

#endif
