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
#ifndef WBE_FILE_ROC_SINGLE_HH
#define WBE_FILE_ROC_SINGLE_HH

#include "core/memory/reference_strong.hh"
#include "function/render/debug_data.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/i_rend_object_container.hh"
#include "function/render/rend_defs.hh"
#include <cstdint>

namespace WhiteBirdEngine {

/**
 * @class ROCSingle
 * @brief Container that resolves the same render object for every slot. Neither copyable nor movable.
 */
class ROCSingle final : public IRendObjectContainer {
public:
    ROCSingle() = delete;
    virtual ~ROCSingle() override = default;
    ROCSingle(const ROCSingle&) = delete;
    ROCSingle(ROCSingle&&) noexcept = delete;
    ROCSingle& operator=(const ROCSingle&) = delete;
    ROCSingle& operator=(ROCSingle&&) noexcept = delete;

    /**
     * @brief Create the render object with a factory.
     * @param p_factory Factory called with slot 0 now and on every recreate().
     * @throws std::runtime_error If p_factory is null or returns null.
     */
    explicit ROCSingle(Factory p_factory);

    virtual void set(uint32_t p_frame_slot, Ref<IRendObject> p_resource) override;

    virtual void clear(uint32_t p_frame_slot) override;

    virtual void clear() override;

    /**
     * @brief Replace the resource.
     *
     * @param p_resource Replacement resource.
     * @throws std::runtime_error If the resource's concrete type differs from the container's.
     */
    void set(Ref<IRendObject> p_resource);

    virtual Ref<IRendObject> get() override;
    virtual Ref<IRendObject> get() const override;
    virtual Ref<IRendObject> get(uint32_t p_frame_slot) override;
    virtual Ref<IRendObject> get(uint32_t p_frame_slot) const override;
    virtual uint32_t get_slot_count() const override;
    virtual DebugDataRows desc_debug_data(RendAlloc* p_alloc) const override;
    virtual void recreate() override;

private:
    Factory factory;
    Ref<IRendObject> resource;
};

} // namespace WhiteBirdEngine
#endif
