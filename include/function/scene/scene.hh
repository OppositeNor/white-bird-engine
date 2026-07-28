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
#ifndef WBE_FILE_SCENE_HH
#define WBE_FILE_SCENE_HH

#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "function/function_utils.hh"
#include "utils/utils.hh"
#include <entt/entt.hpp>

namespace WhiteBirdEngine {

class SceneNode;

/**
 * @class Scene
 * @brief Scene class.
 */
class Scene {
public:
    Scene(HeapAllocatorDefault* p_allocator);
    ~Scene();
    Scene(const Scene&) = delete;
    Scene(Scene&&) noexcept = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&&) noexcept = delete;

    /**
     * @brief Get the allocator of the scene.
     *
     * @return The allocator of the scene.
     */
    HeapAllocatorDefault* get_allocator() {
        return allocator;
    }

    /**
     * @brief Get the allocator of the scene.
     *
     * @return The allocator of the scene.
     */
    const HeapAllocatorDefault* get_allocator() const {
        return allocator;
    }

    /**
     * @brief Get the registry of the scene entities.
     *
     * @return The registry of the scene entities.
     */
    entt::registry& get_entities() {
        return entities;
    }

    /**
     * @brief Get the registry of the scene entities.
     *
     * @return The registry of the scene entities.
     */
    const entt::registry& get_entities() const {
        return entities;
    }

    /**
     * @brief Get the component list for a component type.
     *
     * @param p_component_type_id The component type id to find.
     * @return The component list, or nullptr if the type has no component list.
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
     * @param p_entity The entity to unregister.
     */
    void unregister_scene_node(EntityID p_entity);

    /**
     * @brief Get the scene node registered to an entity.
     *
     * @param p_entity The entity to query.
     * @return The scene node registered to the entity, or null if none exists.
     */
    Ref<SceneNode> get_scene_node(EntityID p_entity);

private:
    void cache_component_lists();

    HeapAllocatorDefault* allocator;
    entt::registry entities;
    HashMap<TypeID, entt::sparse_set*, HeapAllocatorDefault> component_lists;
    HashMap<EntityID, RefWeak<SceneNode>, HeapAllocatorDefault> scene_nodes;
};

} // namespace WhiteBirdEngine

#endif
