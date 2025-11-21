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
#ifndef __WBE_SCENE_NODE_HH__
#define __WBE_SCENE_NODE_HH__

#include "scene.hh"
#include "core/memory/reference_strong.hh"
#include "core/object/object.hh"
#include "core/reflection/reflection_defs.hh"
#include "global/stl_allocator.hh"
#include <stdexcept>
namespace WhiteBirdEngine {
/**
 * @class SceneNode
 * @brief A node of the scene.
 *
 * @todo Test
 */
WBE_CLASS(SceneNode) : public Object {
protected:
public:

    WBE_DECL_SERIALIZABLE(SceneNode);

    virtual ~SceneNode() {}
    SceneNode(const SceneNode&) = delete;
    SceneNode(SceneNode&& p_other)
        : name(std::move(p_other.name)), parent(std::move(p_other.parent)), children(std::move(p_other.children)) {
    }
    SceneNode& operator=(const SceneNode&) = delete;
    SceneNode& operator=(SceneNode&& p_other) {
        parent = std::move(p_other.parent);
        children = std::move(p_other.children);
        return *this;
    }

    /**
     * @brief Constructor.
     *
     * @param p_scene The scene of this node lives in.
     */
    SceneNode(Ref<Scene> p_scene)
        : name(p_scene->get_allocator()), children(p_scene->get_allocator()) {}

    /**
     * @brief Set the reference referencing this instance.
     *
     * @param p_ref_of_this The reference referencing this instance.
     */
    void set_ref_of_this(Ref<SceneNode> p_ref_of_this) {
        WBE_DEBUG_ASSERT(this_ref.is_null());
        this_ref = p_ref_of_this;
    }

    /**
     * @brief Remove a node in chilid.
     *
     * @param p_node The node to be removed.
     */
    void remove_children(Ref<SceneNode> p_node) {
        for (auto iter = children.begin(); iter != children.end(); ++iter) {
            if (*iter == p_node) {
                children.erase(iter);
                return;
            }
        }
        throw std::runtime_error("Children to be removed not found.");
    }

    /**
     * @brief Check if a node contains a children.
     *
     * @param p_node The node to check.
     * @return True if the node has the children, false otherwise.
     */
    bool has_children(Ref<SceneNode> p_node) {
        for (auto iter = children.begin(); iter != children.end(); ++iter) {
            if (*iter == p_node) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Set the parent of this node.
     *
     * @param p_parent The parent node to set to.
     * @param p_ref_of_this The reference of this.
     */
    void set_parent(Ref<SceneNode> p_parent) {
        WBE_DEBUG_ASSERT(!this_ref.is_null());
        if (!parent.is_null()) {
            parent.lock()->remove_children(this_ref.lock());
        }
        parent = p_parent;
        p_parent->children.push_back(this_ref.lock());
    }


private:
    using string = string<>;

    RefWeak<SceneNode> this_ref;

    string name;
    RefWeak<SceneNode> parent;
    vector<Ref<SceneNode>> children;
};

}


#endif
