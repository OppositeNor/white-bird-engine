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
#ifndef WBE_FILE_RENDER_SOFTWARE_DEFS
#define WBE_FILE_RENDER_SOFTWARE_DEFS

#include "core/allocator/stl_allocator.hh"
#include "function/render/render_defs.hh"
#include <cstdint>
namespace WhiteBirdEngine {

struct VertexInputSoftware {
    VertexInputSoftware(RenderAllocator* p_allocator)
        : buffer(p_allocator), layout(p_allocator) {}
    struct ElementDescription {
        enum class Type {
            FLOAT32,
            INT8,
        };
        Type type{};
        uint32_t element_stride = 0;
        uint32_t element_count = 0;
    };

    Vector<uint8_t> buffer;
    Vector<ElementDescription> layout;
    uint32_t vertex_stride = 0;
    uint32_t element_count = 0;
};

struct BaseRenderDataSoftware {
    BaseRenderDataSoftware(RenderAllocator* p_allocator)
        : main_allocator(p_allocator), per_thread_allocators(p_allocator), vertex_input(p_allocator) {}
    RenderAllocator* main_allocator = nullptr;
    Vector<RenderAllocator*> per_thread_allocators;

    VertexInputSoftware vertex_input;
};

} // namespace WhiteBirdEngine

#endif
