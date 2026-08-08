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
#ifndef WBE_FILE_SCENE_NODE_HH
#define WBE_FILE_SCENE_NODE_HH

#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/function_utils.hh"
#include "utils/utils.hh"
#include "world.hh"
namespace WhiteBirdEngine {
/**
 * @class SceneNode
 * @brief A node of the scene.
 *
 * @todo Test
 */
class WBE_META() SceneNode final : public ThisRef<SceneNode> {
protected:
public:
    SceneNode() = delete;
    virtual ~SceneNode() override;
    SceneNode(const SceneNode&) = delete;
    SceneNode(SceneNode&& p_other) noexcept;
    SceneNode& operator=(const SceneNode&) = delete;
    SceneNode& operator=(SceneNode&& p_other) noexcept;

    /**
     * @brief Set the reference referencing this instance.
     *
     * @param p_ref_of_this The reference referencing this instance.
     */
    void set_ref_of_this(Ref<SceneNode> p_ref_of_this);

    /**
     * @brief Constructor.
     *
     * @param p_world The world this node lives in.
     */
    SceneNode(Ref<World> p_world);

    /**
     * @brief Get the entity instance owned by this node.
     *
     * @return The entity instance owned by this node.
     */
    EntityID get_entity() const {
        return entity;
    }

    /**
     * @brief Get the world this node lives in.
     *
     * @return The world owning this node's entity.
     */
    Ref<World> get_world() const {
        return world;
    }

    /**
     * @brief Remove a component from this node's entity.
     *
     * @param p_component_type_id The component type id to remove.
     */
    void remove_component(TypeID p_component_type_id);

    /**
     * @brief Check if this node's entity has a component.
     *
     * @param p_component_type_id The component type id to check.
     * @return True if the node has the component type, false otherwise.
     */
    bool has_component(TypeID p_component_type_id);

    /**
     * @brief Get this node's entity component type list.
     *
     * @return This node's entity component type list.
     */
    Vector<TypeID, HeapAllocatorDefault> get_component_type_ids();

private:
    void destroy_entity();

    String<HeapAllocatorDefault> name;
    Ref<World> world;
    EntityID entity = entt::null;
};

} // namespace WhiteBirdEngine

#endif
