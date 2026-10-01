/* Copyright 2026 OppositeNor

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
#ifndef WBE_FILE_MESH_DRAW_BATCH_VK_HH
#define WBE_FILE_MESH_DRAW_BATCH_VK_HH

#include "function/render_vk/vertex_input_description_vk.hh"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

/**
 * @struct MeshDrawBatchVK
 * @brief One compatible range in a compiled mesh's indexed-indirect command buffer. Copyable and movable.
 */
struct MeshDrawBatchVK {
    MeshDrawBatchVK() = default;
    ~MeshDrawBatchVK() = default;
    MeshDrawBatchVK(const MeshDrawBatchVK&) = default;
    MeshDrawBatchVK(MeshDrawBatchVK&&) noexcept = default;
    MeshDrawBatchVK& operator=(const MeshDrawBatchVK&) = default;
    MeshDrawBatchVK& operator=(MeshDrawBatchVK&&) noexcept = default;

    /** First tightly packed indirect command in the batch buffer. */
    uint32_t first_draw = 0;
    /** Number of tightly packed indirect commands. */
    uint32_t draw_count = 0;
    /** Dynamic vertex-input layout shared by the commands. */
    VertexInputDescriptionVK vertex_input;
    /** Shared byte offset used when binding the vertex buffer. */
    VkDeviceSize vertex_buffer_offset = 0;
};

} // namespace WhiteBirdEngine

#endif
