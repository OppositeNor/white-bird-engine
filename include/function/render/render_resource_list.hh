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
#ifndef WBE_FILE_RENDER_RESOURCE_LIST_HH
#define WBE_FILE_RENDER_RESOURCE_LIST_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_object.hh"
#include "function/render/render_defs.hh"
#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class RenderResourceList
 * @brief CPU-side ordered group of render objects that can be bound to a render graph as one aggregate resource.
 *
 * The list retains shared ownership of its entries but does not create GPU resources. It is neither copyable nor movable;
 * share it through `Ref<RenderResourceList>`.
 */
class RenderResourceList final {
public:
    RenderResourceList() = delete;
    ~RenderResourceList() = default;
    RenderResourceList(const RenderResourceList&) = delete;
    RenderResourceList(RenderResourceList&&) noexcept = delete;
    RenderResourceList& operator=(const RenderResourceList&) = delete;
    RenderResourceList& operator=(RenderResourceList&&) noexcept = delete;

    /**
     * @brief Construct an empty resource list.
     * @param p_allocator Allocator retained by the internal vector; it must outlive this list.
     * @throws std::runtime_error If the allocator is null.
     */
    explicit RenderResourceList(RenderAllocator* p_allocator);

    /**
     * @brief Append one render object.
     * @param p_resource Resource whose shared ownership is transferred into the list.
     * @return This resource list.
     * @throws std::runtime_error If the resource is null.
     */
    RenderResourceList& add_resource(Ref<IRenderObject> p_resource);

    /**
     * @brief Get the number of retained resources.
     * @return Resource count.
     */
    size_t get_resource_count() const;

    /**
     * @brief Acquire shared ownership of one retained resource.
     * @param p_index Zero-based resource index.
     * @return The indexed render object.
     * @throws std::runtime_error If the index is out of bounds.
     */
    Ref<IRenderObject> acquire_resource(size_t p_index) const;

private:
    Vector<Ref<IRenderObject>, RenderAllocator> resources;
};

} // namespace WhiteBirdEngine

#endif
