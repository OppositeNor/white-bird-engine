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
#ifndef WBE_FILE_STATIC_SET_BUFFER_COMPILER_VK_HH
#define WBE_FILE_STATIC_SET_BUFFER_COMPILER_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @class StaticSetBufferCompilerVK
 * @brief Validates one static geometry set and compiles its shared vertex and index buffers for Vulkan use.
 */
class StaticSetBufferCompilerVK final {
public:
    /**
     * @class BuildResult
     * @brief Shared Vulkan buffers compiled for one static geometry set.
     */
    struct BuildResult {
        Ref<ROBufferVK> vertex_buffer = MEM_NULL;
        Ref<ROBufferVK> index_buffer = MEM_NULL;
    };

    StaticSetBufferCompilerVK() = delete;
    ~StaticSetBufferCompilerVK() = default;
    StaticSetBufferCompilerVK(const StaticSetBufferCompilerVK&) = default;
    StaticSetBufferCompilerVK(StaticSetBufferCompilerVK&&) noexcept = default;
    StaticSetBufferCompilerVK& operator=(const StaticSetBufferCompilerVK&) = default;
    StaticSetBufferCompilerVK& operator=(StaticSetBufferCompilerVK&&) noexcept = default;

    /**
     * @brief Construct a compiler bound to one renderer's Vulkan state.
     */
    explicit StaticSetBufferCompilerVK(const BaseRenderDataVK* p_render_data);

    /**
     * @brief Compile or reuse shared draw buffers for one static geometry set.
     *
     * Returns empty references when the set has no drawable submeshes.
     */
    BuildResult build(RefRes<RUStaticGeometrySet> p_set, VkBufferUsageFlags p_vertex_usage, VkBufferUsageFlags p_index_usage) const;

private:
    const BaseRenderDataVK* render_data = nullptr;
};

} // namespace WhiteBirdEngine

#endif