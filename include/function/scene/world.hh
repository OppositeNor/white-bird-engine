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
#ifndef WBE_FILE_WORLD_HH
#define WBE_FILE_WORLD_HH

#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "function/function_utils.hh"
#include "function/render/render_resource_unit.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <entt/entt.hpp>
#include <string_view>

namespace WhiteBirdEngine {

class SceneNode;

/**
 * @class World
 * @brief Owns the local ECS data and render bindings for a world.
 *
 * The allocator and non-owning static mesh reference must outlive the world.
 * Worlds are neither copyable nor movable.
 */
class World {
public:
    /**
     * @brief Construct an empty world.
     *
     * @param p_allocator Allocator used by world-owned containers and strings.
     */
    World(HeapAllocatorDefault* p_allocator);
    ~World();
    World(const World&) = delete;
    World(World&&) noexcept = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) noexcept = delete;

    /**
     * @brief Get the allocator of the world.
     *
     * @return The allocator used by the world.
     */
    HeapAllocatorDefault* get_allocator() {
        return allocator;
    }

    /**
     * @brief Get the allocator of the world.
     *
     * @return The allocator used by the world.
     */
    const HeapAllocatorDefault* get_allocator() const {
        return allocator;
    }

    /**
     * @brief Get the registry of the world entities.
     *
     * @return The mutable entity registry owned by the world.
     */
    entt::registry& get_entities() {
        return entities;
    }

    /**
     * @brief Get the registry of the world entities.
     *
     * @return The entity registry owned by the world.
     */
    const entt::registry& get_entities() const {
        return entities;
    }

    /**
     * @brief Get the component list for a component type.
     *
     * @param p_component_type_id The component type id to find.
     * @return The component list, or nullptr when the type has no component list.
     */
    entt::sparse_set* get_component_list(TypeID p_component_type_id);

    /**
     * @brief Get the component type ids attached to an entity.
     *
     * @param p_entity The entity to inspect.
     * @return The component type ids attached to the entity.
     */
    Vector<TypeID, HeapAllocatorDefault> get_component_type_ids(EntityID p_entity);

    /**
     * @brief Register a scene node for entity lookup.
     *
     * @param p_scene_node The scene node to register.
     */
    void register_scene_node(Ref<SceneNode> p_scene_node);

    /**
     * @brief Unregister the scene node of an entity.
     *
     * @param p_entity The entity whose scene node should be unregistered.
     */
    void unregister_scene_node(EntityID p_entity);

    /**
     * @brief Get the scene node registered to an entity.
     *
     * @param p_entity The entity to query.
     * @return The registered scene node, or null when none exists.
     */
    Ref<SceneNode> get_scene_node(EntityID p_entity);

    /**
     * @brief Set the resource key of the world's render graph.
     *
     * @param p_resource_key The render graph resource key to associate with the world.
     */
    void set_render_graph_resource_key(std::string_view p_resource_key);

    /**
     * @brief Get the resource key of the world's render graph.
     *
     * @return The render graph resource key, or an empty view when no graph is set.
     */
    std::string_view get_render_graph_resource_key() const {
        return render_graph_resource_key;
    }

    /**
     * @brief Set the non-owning static opaque mesh resource.
     *
     * @param p_mesh The non-owning mesh resource to publish for rendering.
     */
    void set_static_opaque_mesh(RefRes<RUMesh3D> p_mesh) {
        static_opaque_mesh = p_mesh;
    }

    /**
     * @brief Get the non-owning static opaque mesh resource.
     *
     * @return The non-owning static opaque mesh resource, or null when no mesh is set.
     */
    RefRes<RUMesh3D> get_static_opaque_mesh() const {
        return static_opaque_mesh;
    }

    /**
     * @brief Set the non-owning static opaque set resource.
     *
     * @param p_set The non-owning static opaque set resource to publish for rendering.
     */
    void set_static_opaque_set(RefRes<RUStaticOpaqueSet> p_set) {
        static_opaque_set = p_set;
    }

    /**
     * @brief Get the non-owning static opaque set resource.
     *
     * @return The non-owning static opaque set resource, or null when no set is set.
     */
    RefRes<RUStaticOpaqueSet> get_static_opaque_set() const {
        return static_opaque_set;
    }

    /**
     * @brief Set the non-owning static masked set resource.
     *
     * @param p_set The non-owning static masked set resource to publish for rendering.
     */
    void set_static_masked_set(RefRes<RUStaticMaskedSet> p_set) {
        static_masked_set = p_set;
    }

    /**
     * @brief Get the non-owning static masked set resource.
     *
     * @return The non-owning static masked set resource, or null when no set is set.
     */
    RefRes<RUStaticMaskedSet> get_static_masked_set() const {
        return static_masked_set;
    }

private:
    void cache_component_lists();

    HeapAllocatorDefault* allocator;
    entt::registry entities;
    HashMap<TypeID, entt::sparse_set*, HeapAllocatorDefault> component_lists;
    HashMap<EntityID, RefWeak<SceneNode>, HeapAllocatorDefault> scene_nodes;
    String<HeapAllocatorDefault> render_graph_resource_key;
    RefRes<RUMesh3D> static_opaque_mesh;
    RefRes<RUStaticOpaqueSet> static_opaque_set;
    RefRes<RUStaticMaskedSet> static_masked_set;
};

} // namespace WhiteBirdEngine

#endif
