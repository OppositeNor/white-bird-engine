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
#ifndef WBE_FILE_ROC_PER_FRAME_VK_HH
#define WBE_FILE_ROC_PER_FRAME_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_defs.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <cstddef>
#include <cstdint>
#include <functional>

namespace WhiteBirdEngine {

/**
 * @class ROCPerFrameVK
 * @brief Container that resolves a different render object for each frame slot.
 */
class ROCPerFrameVK final : public RenderObjectContainerVK {
public:
    using Factory = std::function<Ref<IRenderObjectVK>(uint32_t)>;

    ROCPerFrameVK() = delete;
    ~ROCPerFrameVK() override = default;
    ROCPerFrameVK(const ROCPerFrameVK&) = delete;
    ROCPerFrameVK(ROCPerFrameVK&&) noexcept = delete;
    ROCPerFrameVK& operator=(const ROCPerFrameVK&) = delete;
    ROCPerFrameVK& operator=(ROCPerFrameVK&&) noexcept = delete;

    explicit ROCPerFrameVK(const BaseRenderDataVK* p_render_data);
    ROCPerFrameVK(const BaseRenderDataVK* p_render_data, Factory p_factory);

    Ref<IRenderObjectVK> get() override;
    Ref<IRenderObjectVK> get() const override;
    Ref<IRenderObjectVK> get(uint32_t p_frame_slot) override;
    Ref<IRenderObjectVK> get(uint32_t p_frame_slot) const override;
    void set(uint32_t p_frame_slot, Ref<IRenderObjectVK> p_resource);

    /**
     * @brief Release the resources retained for all frame slots.
     */
    void clear();

    uint32_t get_context_count() const;

    /**
     * @brief Publish the latest buffer contents for lazy per-frame application.
     *
     * The call and subsequent resource resolution must be externally synchronized. Each frame slot applies the newest published
     * bytes once when it is next resolved, allowing intermediate updates to be skipped without losing the latest state.
     *
     * @param p_data Bytes to copy into each frame-slot buffer.
     * @param p_dst_offset Destination offset in each buffer.
     * @param p_size Number of bytes to copy.
     */
    void publish_buffer_update(const void* p_data, size_t p_dst_offset, size_t p_size);

private:
    void synchronize(uint32_t p_frame_slot) const;

    const BaseRenderDataVK* render_data = nullptr;
    Vector<Ref<IRenderObjectVK>, RenderAllocator> resources;
    mutable Vector<uint64_t, RenderAllocator> applied_generations;
    Vector<uint8_t, RenderAllocator> pending_buffer_data;
    uint64_t update_generation = 0;
    size_t pending_buffer_dst_offset = 0;
};

} // namespace WhiteBirdEngine

#endif
