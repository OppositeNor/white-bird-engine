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
#ifndef WBE_FILE_ROC_MULTISLOTS_HH
#define WBE_FILE_ROC_MULTISLOTS_HH

#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/rend_defs.hh"
#include "function/render/debug_data.hh"
#include "function/render/i_rend_object_container.hh"
#include <cstddef>
#include <cstdint>
#include <functional>

namespace WhiteBirdEngine {

/**
 * @class ROCMultislots
 * @brief Container that resolves a different render object for each configured slot. Neither copyable nor movable.
 *
 * This general-purpose container is widely used to hold per-frame resources, with one slot for each frame in flight.
 */
class ROCMultislots final : public IRendObjectContainer {
public:
    ROCMultislots() = delete;
    virtual ~ROCMultislots() override = default;
    ROCMultislots(const ROCMultislots&) = delete;
    ROCMultislots(ROCMultislots&&) noexcept = delete;
    ROCMultislots& operator=(const ROCMultislots&) = delete;
    ROCMultislots& operator=(ROCMultislots&&) noexcept = delete;

    /**
     * @brief Create and populate an explicit number of slots.
     * @param p_alloc Non-owning allocator used by the container's storage; it must outlive the container.
     * @param p_slot_count Number of slots to create.
     * @param p_factory Factory invoked once for each slot index now and on every recreate().
     * @param p_flush Whether published buffer updates are applied lazily when each slot is resolved.
     * @throws std::runtime_error If p_alloc or p_factory is null, p_slot_count is zero, or p_factory returns null.
     */
    ROCMultislots(RendAlloc* p_alloc, uint32_t p_slot_count, Factory p_factory, bool p_flush = true);

    /**
     * @brief Resolve the first slot.
     * @return The resource stored at slot zero.
     */
    virtual Ref<IRendObject> get() override;

    /**
     * @brief Resolve the first slot.
     * @return The resource stored at slot zero.
     */
    virtual Ref<IRendObject> get() const override;
    virtual Ref<IRendObject> get(uint32_t p_slot) override;
    virtual Ref<IRendObject> get(uint32_t p_slot) const override;
    virtual DebugDataRows desc_debug_data(RendAlloc* p_alloc) const override;
    virtual void set(uint32_t p_slot, Ref<IRendObject> p_resource) override;

    /**
     * @brief Release the resource retained for one slot.
     *
     * @param p_slot Slot index to clear.
     */
    virtual void clear(uint32_t p_slot) override;

    /**
     * @brief Release the resources retained for all slots.
     */
    virtual void clear() override;

    virtual uint32_t get_slot_count() const override;

    /**
     * @brief Replace the render object of every slot with a new one from the factory.
     *
     * With flush enabled, the newest published buffer update is applied again to every new slot when it is next resolved.
     *
     * @throws std::runtime_error If the factory returns null for a slot.
     */
    virtual void recreate() override;

    /**
     * @brief Publish the latest buffer contents for lazy per-slot application.
     *
     * The call and subsequent resource resolution must be externally synchronized. Each slot applies the newest published
     * bytes once when it is next resolved, allowing intermediate updates to be skipped without losing the latest state. This
     * is widely used to update per-frame buffers independently.
     *
     * @param p_data Bytes to copy into each slot's buffer.
     * @param p_dst_offset Destination offset in each buffer.
     * @param p_size Number of bytes to copy.
     * @throws std::runtime_error If the data is empty, or a slot that applies it holds no ROBuffer.
     */
    void publish_buffer_update(const void* p_data, size_t p_dst_offset, size_t p_size);

private:
    void synchronize(uint32_t p_slot) const;

    RendAlloc* alloc = nullptr;
    Factory factory;
    bool flush = true;
    Vector<Ref<IRendObject>, RendAlloc> resources;
    mutable Vector<uint64_t, RendAlloc> applied_generations;
    Vector<uint8_t, RendAlloc> pending_buffer_data;
    uint64_t update_generation = 0;
    size_t pending_buffer_dst_offset = 0;
};

} // namespace WhiteBirdEngine

#endif
