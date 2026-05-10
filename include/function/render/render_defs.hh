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
#ifndef WBE_FILE_RENDER_DEFS_HH
#define WBE_FILE_RENDER_DEFS_HH

#include "core/allocator/heap_allocator_aligned_pool_impl_list.hh"
#include <cstddef>
#include <string>

namespace WhiteBirdEngine {
using RenderAllocator = HeapAllocatorAlignedPoolImplicitList;

/**
 * @class RenderUniformBinding
 * @brief Caller-provided bytes for one reflected shader uniform field.
 */
struct RenderUniformBinding {
    /**
     * @brief Reflected shader field path, e.g. "camera_info.view".
     */
    std::string field_path;
    /**
     * @brief Pointer to the source field data. Data is copied during compilation.
     */
    const void* data = nullptr;
    /**
     * @brief Byte size of the source field data.
     */
    size_t size = 0;
};

} // namespace WhiteBirdEngine

#endif
