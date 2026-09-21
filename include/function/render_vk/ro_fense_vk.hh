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
#ifndef WBE_FILE_RO_FENSE_VK_HH
#define WBE_FILE_RO_FENSE_VK_HH

#include "function/render_vk/render_vk_defs.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class ROFenseVK
 * @brief RAII wrapper for a Vulkan fence used to observe GPU-job completion.
 *
 * Waiting is thread-safe and may be performed by an engine worker. The fence must not outlive the renderer that created it.
 * Copying and moving are disabled; share ownership with Ref.
 */
class ROFenseVK final {
public:
    ROFenseVK() = delete;
    ~ROFenseVK();
    ROFenseVK(const ROFenseVK&) = delete;
    ROFenseVK(ROFenseVK&&) noexcept = delete;
    ROFenseVK& operator=(const ROFenseVK&) = delete;
    ROFenseVK& operator=(ROFenseVK&&) noexcept = delete;

    /**
     * @brief Create an unsignaled fence.
     * @param p_render_data Renderer data that owns the Vulkan device and delayed-destruction queue.
     */
    explicit ROFenseVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Wait indefinitely for the submitted GPU job to finish.
     * @throws std::runtime_error if Vulkan cannot complete the wait.
     */
    void wait() const;

    /**
     * @brief Get the wrapped Vulkan fence for queue submission.
     * @return The wrapped fence.
     */
    VkFence get_fense() const {
        return fense;
    }

private:
    const BaseRenderDataVK* render_data = nullptr;
    VkFence fense = VK_NULL_HANDLE;
};

} // namespace WhiteBirdEngine

#endif
