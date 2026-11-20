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
#ifndef WBE_FILE_I_BINDABLE_REND_RESOURCE_VK_HH
#define WBE_FILE_I_BINDABLE_REND_RESOURCE_VK_HH

#include "core/memory/reference_strong.hh"
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

struct RendGraphDescVK;
class IRendObjectVK;
class RendObjectContainerVK;
class RendResourceListVK;

/**
 * @class IBindableRendResourceVK
 * @brief Pure interface of resources that can be imported into a Vulkan render graph under a caller-chosen key.
 *
 * A single render object or container is bound under the key itself. A composite binds each member under
 * `<key>.<member>` (see make_member_resource_id()). Binding retains strong references to every imported render object, so
 * the bound object itself need not outlive the graph descriptor. Implementations are confined to the rendering thread. The
 * interface holds no state, so it may be combined with other bases through multiple inheritance; it must never be held
 * through Ref, because Ref does not adjust pointers to non-primary bases.
 */
class IBindableRendResourceVK {
public:
    virtual ~IBindableRendResourceVK() = default;

    /**
     * @brief Import this resource into p_graph under p_resource_id.
     * @throws std::runtime_error If p_resource_id is empty, a required member is missing, or the resource is not owned by
     * a Ref when it imports itself.
     */
    virtual void bind(RendGraphDescVK& p_graph, std::string_view p_resource_id) const = 0;

    /**
     * @brief Compose the graph resource id of one member of a composite bound under p_resource_id.
     * @return `<p_resource_id>.<p_member>`.
     */
    static std::string make_member_resource_id(std::string_view p_resource_id, std::string_view p_member);

protected:
    IBindableRendResourceVK() = default;
    IBindableRendResourceVK(const IBindableRendResourceVK&) = default;
    IBindableRendResourceVK(IBindableRendResourceVK&&) noexcept = default;
    IBindableRendResourceVK& operator=(const IBindableRendResourceVK&) = default;
    IBindableRendResourceVK& operator=(IBindableRendResourceVK&&) noexcept = default;

    /**
     * @brief Import one render object under p_resource_id, wrapped in a single-slot container.
     *
     * An object without a debug name is named p_resource_id. Call on the rendering thread.
     * @throws std::runtime_error If p_resource is null or p_resource_id is empty.
     */
    static void bind_rend_object(RendGraphDescVK& p_graph, std::string_view p_resource_id, Ref<IRendObjectVK> p_resource);

    /**
     * @brief Import one container under p_resource_id.
     * @throws std::runtime_error If p_container is null or p_resource_id is empty.
     */
    static void bind_container(RendGraphDescVK& p_graph, std::string_view p_resource_id, Ref<RendObjectContainerVK> p_container);

    /**
     * @brief Import an aggregate resource list under p_resource_id; its entries are registered as `<p_resource_id>[<index>]`.
     * @throws std::runtime_error If p_resource_list is null or p_resource_id is empty.
     */
    static void bind_resource_list(RendGraphDescVK& p_graph, std::string_view p_resource_id, Ref<RendResourceListVK> p_resource_list);
};

} // namespace WhiteBirdEngine

#endif
