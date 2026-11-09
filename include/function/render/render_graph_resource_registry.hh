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
#ifndef WBE_FILE_RENDER_GRAPH_RESOURCE_REGISTRY_HH
#define WBE_FILE_RENDER_GRAPH_RESOURCE_REGISTRY_HH

#include "core/allocator/stl_allocator.hh"
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "function/render/render_defs.hh"
#include "utils/utils.hh"
#include <functional>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

class IRenderGraph;

/**
 * @class RenderGraphResourceRegistry
 * @brief The registration bookkeeping of a render graph; IRenderGraph implementations forward to it. Neither copyable
 * nor movable.
 *
 * Maps description types to their registrators, and registration keys to a copy of their description and the registered
 * object, which it holds strongly. See IRenderGraph for the registration contract. Registrators may
 * register or look up other keys while compiling. Confined to the rendering thread.
 */
class RenderGraphResourceRegistry final {
public:
    /**
     * @brief Called with every object the registry stops holding because it was replaced or unregistered, to release it
     * once the frames that may use it have completed. Without one, the object is released immediately.
     */
    using ReleaseCallback = std::function<void(Ref<IRenderObject>)>;

    RenderGraphResourceRegistry() = delete;
    ~RenderGraphResourceRegistry() = default;
    RenderGraphResourceRegistry(const RenderGraphResourceRegistry&) = delete;
    RenderGraphResourceRegistry(RenderGraphResourceRegistry&&) noexcept = delete;
    RenderGraphResourceRegistry& operator=(const RenderGraphResourceRegistry&) = delete;
    RenderGraphResourceRegistry& operator=(RenderGraphResourceRegistry&&) noexcept = delete;

    /**
     * @brief Construct an empty registry.
     *
     * @param p_allocator The allocator of the registry's storage and description copies; it must outlive the registry.
     * @param p_owner The graph passed to registrators; it must outlive the registry.
     * @param p_release The release callback, or null to release replaced objects immediately.
     * @throws std::runtime_error If the allocator or the owner is null.
     */
    RenderGraphResourceRegistry(RenderAllocator* p_allocator, IRenderGraph* p_owner, ReleaseCallback p_release = nullptr);

    /**
     * @brief See IRenderGraph::add_resource_registrator().
     */
    void add_registrator(Unique<IResourceRegistrator> p_registrator);

    /**
     * @brief See IRenderGraph::has_resource_registrator().
     */
    bool has_registrator(TypeID p_description_type) const;

    /**
     * @brief See IRenderGraph::register_resource().
     */
    Ref<IRenderObject> register_resource(std::string_view p_key, const IResourceDescription& p_description);

    /**
     * @brief See IRenderGraph::register_resource().
     */
    Ref<IRenderObject> register_resource(std::string_view p_key, TypeID p_description_type, const DataValue& p_description);

    /**
     * @brief See IRenderGraph::unregister_resource().
     */
    void unregister_resource(std::string_view p_key);

    /**
     * @brief See IRenderGraph::contains_registered_resource().
     */
    bool contains(std::string_view p_key) const;

    /**
     * @brief See IRenderGraph::try_acquire_registered_resource().
     */
    Ref<IRenderObject> try_get(std::string_view p_key) const;

    /**
     * @brief Get the description a key is registered with.
     *
     * @throws std::runtime_error If the key is not registered.
     */
    const IResourceDescription& get_description(std::string_view p_key) const;

private:
    struct Entry {
        Unique<IResourceDescription> description;
        Ref<IRenderObject> object;
    };

    RenderAllocator* allocator;
    IRenderGraph* owner;
    ReleaseCallback release;
    Map<TypeID, Unique<IResourceRegistrator>, RenderAllocator> registrators;
    Map<std::string, Entry, RenderAllocator> entries;

    IResourceRegistrator& find_registrator(TypeID p_description_type, std::string_view p_key);
    void release_object(Ref<IRenderObject> p_object) const;
};

} // namespace WhiteBirdEngine

#endif
