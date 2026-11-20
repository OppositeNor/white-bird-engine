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
#ifndef WBE_FILE_REND_RESOURCE_LIST_VK_HH
#define WBE_FILE_REND_RESOURCE_LIST_VK_HH

#include "core/alloc/stl_alloc.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_object.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include <cstddef>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class RendResourceListVK
 * @brief CPU-side ordered group of render objects that can be bound to a render graph as one aggregate resource.
 *
 * The list retains shared ownership of its entries but does not create GPU resources. It is neither copyable nor movable;
 * share it through `Ref<RendResourceListVK>`. Binding registers the aggregate under the requested id and every entry as
 * `<id>[<index>]`.
 */
class RendResourceListVK final : public ThisRef<RendResourceListVK>, public IBindableRendResourceVK {
public:
    RendResourceListVK() = delete;
    ~RendResourceListVK() = default;
    RendResourceListVK(const RendResourceListVK&) = delete;
    RendResourceListVK(RendResourceListVK&&) noexcept = delete;
    RendResourceListVK& operator=(const RendResourceListVK&) = delete;
    RendResourceListVK& operator=(RendResourceListVK&&) noexcept = delete;

    /**
     * @brief Construct an empty resource list.
     * @param p_alloc Allocator retained by the internal vector; it must outlive this list.
     * @throws std::runtime_error If the allocator is null.
     */
    explicit RendResourceListVK(RendAlloc* p_alloc);

    /**
     * @brief Append one render object.
     * @param p_resource Resource whose shared ownership is transferred into the list.
     * @return This resource list.
     * @throws std::runtime_error If the resource is null.
     */
    RendResourceListVK& add_resource(Ref<IRendObject> p_resource);

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
    Ref<IRendObject> acquire_resource(size_t p_index) const;

    /**
     * @brief Import this list under p_resource_id.
     * @throws std::runtime_error If the list was not created with make_ref() or p_resource_id is empty.
     */
    virtual void bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const override;

private:
    Vector<Ref<IRendObject>, RendAlloc> resources;
};

} // namespace WhiteBirdEngine

#endif
