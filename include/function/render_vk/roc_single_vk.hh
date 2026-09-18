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
#ifndef WBE_FILE_ROC_SINGLE_VK_HH
#define WBE_FILE_ROC_SINGLE_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"

namespace WhiteBirdEngine {

/**
 * @class ROCSingleVK
 * @brief Container that always resolves the same render object.
 */
class ROCSingleVK final : public RenderObjectContainerVK {
public:
    ROCSingleVK() = delete;
    ~ROCSingleVK() override = default;
    ROCSingleVK(const ROCSingleVK&) = delete;
    ROCSingleVK(ROCSingleVK&&) noexcept = delete;
    ROCSingleVK& operator=(const ROCSingleVK&) = delete;
    ROCSingleVK& operator=(ROCSingleVK&&) noexcept = delete;

    explicit ROCSingleVK(Ref<IRenderObjectVK> p_resource);

    Ref<IRenderObject> get() override;
    Ref<IRenderObject> get() const override;
    Ref<IRenderObject> get(uint32_t p_frame_slot) override;
    Ref<IRenderObject> get(uint32_t p_frame_slot) const override;
    DebugDataRows descriptor_debug_data() const override;

private:
    Ref<IRenderObjectVK> resource;
};

} // namespace WhiteBirdEngine

#endif /* Copyright 2025 OppositeNor                                                                                        \
                                                                                                                            \
    Licensed under the Apache License, Version 2.0 (the "License");                                                         \
    you may not use this file except in compliance with the License.                                                        \
    You may obtain a copy of the License at                                                                                 \
                                                                                                                            \
        http://www.apache.org/licenses/LICENSE-2.0                                                                          \
                                                                                                                            \
    Unless required by applicable law or agreed to in writing, software                                                     \
    distributed under the License is distributed on an "AS IS" BASIS,                                                       \
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                                                \
    See the License for the specific language governing permissions and                                                     \
    limitations under the License.                                                                                          \
 */
#ifndef WBE_FILE_ROC_SINGLE_VK_HH
#define WBE_FILE_ROC_SINGLE_VK_HH

#include "core/memory/reference_strong.hh"
#include "function/render_vk/debug_data_vk.hh"
#include "function/render_vk/i_render_object_vk.hh"
#include "function/render_vk/render_object_container_vk.hh"
#include <cstdint>

namespace WhiteBirdEngine {

/**
 * @class ROCSingleVK
 * @brief Container that always resolves to one fixed render object.
 */
class ROCSingleVK final : public RenderObjectContainerVK {
public:
    ROCSingleVK() = delete;
    ~ROCSingleVK() override = default;
    ROCSingleVK(const ROCSingleVK&) = delete;
    ROCSingleVK(ROCSingleVK&&) noexcept = delete;
    ROCSingleVK& operator=(const ROCSingleVK&) = delete;
    ROCSingleVK& operator=(ROCSingleVK&&) noexcept = delete;

    explicit ROCSingleVK(Ref<IRenderObjectVK> p_resource);

    Ref<IRenderObject> get() override;
    Ref<IRenderObject> get() const override;
    Ref<IRenderObject> get(uint32_t p_frame_slot) override;
    Ref<IRenderObject> get(uint32_t p_frame_slot) const override;
    DebugDataRows descriptor_debug_data() const override;

private:
    Ref<IRenderObjectVK> resource;
};

} // namespace WhiteBirdEngine

#endif
