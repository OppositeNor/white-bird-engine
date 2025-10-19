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
#ifndef __WBE_RENDERER_HH__
#define __WBE_RENDERER_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "utils/interface/singleton.hh"
namespace WhiteBirdEngine {

using RenderAllocator = HeapAllocatorAlignedPool;

/**
 * @class Renderer
 * @brief Renderer interface for different types of renderers, usually refer
 * to the rendering API.
 *
 */
class Renderer : public Singleton<Renderer> {
public:
    Renderer() = default;
    virtual ~Renderer() {}

    /**
     * @brief Called every frame.
     *
     * @param p_delta The difference between the last two calls of this function.
     */
    virtual void tick(double p_delta) = 0;
protected:

private:
    void initialize_render_pass();

};

}

#endif
