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
#ifndef WBE_FILE_OCT_TREE_HH
#define WBE_FILE_OCT_TREE_HH

#include "core/allocator/heap_allocator.hh"
#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/reference_weak.hh"
#include "glm/ext/vector_float3.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
namespace WhiteBirdEngine {

/**
 * @class OctTree
 * @brief Stores values in nodes of a cubic octree.
 *
 * The root is centered at the supplied world position and has the supplied edge length. The tree owns its nodes, while
 * the allocator must outlive the tree and all node references returned by get_node(). Inserting at an occupied node
 * replaces its value. A node may have both a value and children. Positions must lie within the root cube, including its
 * faces. Trees can be moved but not copied.
 *
 * @tparam T The type of the values in the nodes.
 */
template <typename T>
class OctTree {
public:
    /**
     * @struct OctTreeNode
     * @brief A node with an optional value and up to eight children.
     * Nodes cannot be copied or moved because their child ownership and parent links must remain connected.
     */
    struct OctTreeNode {
        /**
         * @brief Create an empty node.
         */
        OctTreeNode() = default;
        /**
         * @brief Release the owned child nodes.
         */
        ~OctTreeNode() = default;
        /**
         * @brief Node ownership cannot be copied.
         */
        OctTreeNode(const OctTreeNode&) = delete;
        /**
         * @brief Node ownership cannot be copied.
         */
        OctTreeNode& operator=(const OctTreeNode&) = delete;
        /**
         * @brief Node identity and parent links cannot be moved.
         */
        OctTreeNode(OctTreeNode&&) = delete;
        /**
         * @brief Node identity and parent links cannot be moved.
         */
        OctTreeNode& operator=(OctTreeNode&&) = delete;

        /**
         * @enum SubRegion
         * @brief Child octants; P and N denote positive and nonpositive offsets from the parent center.
         */
        enum SubRegion {
            PX_PY_PZ,
            PX_PY_NZ,
            PX_NY_PZ,
            PX_NY_NZ,
            NX_PY_PZ,
            NX_PY_NZ,
            NX_NY_PZ,
            NX_NY_NZ,
            MAX_SUBREGION
        };
        /** The value stored directly in this node, if any. */
        std::optional<T> value = std::nullopt;
        /** The non-owning link to the parent; empty for the root. */
        RefWeak<const OctTreeNode> parent;
        /** The owning links to child nodes, indexed by SubRegion. */
        std::array<Ref<OctTreeNode>, MAX_SUBREGION> children;
    };

    /**
     * @brief Create an empty tree with an allocated root node.
     * @param p_allocator Allocator that must outlive the tree and returned node references.
     * @param p_center Finite center of the root cube in world coordinates.
     * @param p_size Positive, finite edge length of the root cube.
     * @throws std::runtime_error If the allocator is null or the center or size is invalid.
     */
    OctTree(HeapAllocator* p_allocator, glm::vec3 p_center, float p_size)
        : allocator(p_allocator), center(p_center), size(p_size) {
        if (allocator == nullptr || !std::isfinite(size) || size <= 0.0F || !std::isfinite(center.x) ||
            !std::isfinite(center.y) || !std::isfinite(center.z)) {
            throw std::runtime_error("OctTree requires a non-null allocator, finite center, and positive, finite size.");
        }
        root = make_ref<OctTreeNode>(allocator);
    }

    /**
     * @brief Release the tree's nodes.
     */
    ~OctTree() = default;
    /**
     * @brief Trees cannot be copied because they own their node hierarchy.
     */
    OctTree(const OctTree&) = delete;
    /**
     * @brief Trees cannot be copied because they own their node hierarchy.
     */
    OctTree& operator=(const OctTree&) = delete;
    /**
     * @brief Transfer ownership of the node hierarchy.
     */
    OctTree(OctTree&&) noexcept = default;
    /**
     * @brief Replace this tree's hierarchy with another tree's hierarchy.
     */
    OctTree& operator=(OctTree&&) noexcept = default;

    /**
     * @brief Store a value at the node selected by a world position and depth.
     * @tparam T1 Type of the value being forwarded.
     * @param p_val Value to store; replaces any value already at that node.
     * @param p_position World position used to select child octants.
     * @param p_depth Number of child levels below the root; zero selects the root.
     * @throws std::runtime_error If the position lies outside the root cube or the requested cubes become too small
     * to represent with float.
     */
    template <typename T1>
    void insert(T1&& p_val, glm::vec3 p_position, uint32_t p_depth) {
        check_position(p_position);
        float child_size = size;
        for (uint32_t i = 0; i < p_depth; ++i) {
            child_size /= 2.0F;
            if (child_size == 0.0F) {
                throw std::runtime_error("OctTree insertion depth exceeds float range.");
            }
        }
        insert(root, std::forward<T1>(p_val), p_position - center, size, p_depth);
    }

    /**
     * @brief Get the node on a position's path at the requested depth, or the deepest existing ancestor.
     * @param p_position World position used to select child octants.
     * @param p_depth Number of child levels to traverse; a negative value searches until a child is absent.
     * @return A read-only reference to the selected node, including an empty root for an empty tree.
     * @throws std::runtime_error If the position lies outside the root cube.
     */
    Ref<const OctTreeNode> get_node(glm::vec3 p_position, int32_t p_depth = -1) const {
        check_position(p_position);
        return get_node(root, p_position - center, size, p_depth);
    }

private:
    HeapAllocator* allocator = nullptr;
    Ref<OctTreeNode> root;
    glm::vec3 center;
    float size;

    void check_position(glm::vec3 p_position) const {
        glm::vec3 relative_position = p_position - center;
        float half_size = size / 2.0F;
        if (!std::isfinite(relative_position.x) || !std::isfinite(relative_position.y) ||
            !std::isfinite(relative_position.z) || std::abs(relative_position.x) > half_size ||
            std::abs(relative_position.y) > half_size || std::abs(relative_position.z) > half_size) {
            throw std::runtime_error("OctTree position lies outside the root cube.");
        }
    }

    static typename OctTreeNode::SubRegion get_subregion(glm::vec3 p_relative_position) {
        int subregion = (p_relative_position.x > 0 ? 0 : 0b100) | (p_relative_position.y > 0 ? 0 : 0b010) |
                        (p_relative_position.z > 0 ? 0 : 0b001);
        return static_cast<typename OctTreeNode::SubRegion>(subregion);
    }

    Ref<const OctTreeNode> get_node(Ref<const OctTreeNode> p_curr, glm::vec3 p_relative_position, float p_curr_size,
                                   int32_t p_depth) const {
        if (p_depth == 0) {
            return p_curr;
        }
        auto subregion = get_subregion(p_relative_position);
        auto child = p_curr->children[subregion];
        if (child == MEM_NULL) {
            return p_curr;
        }
        auto half_size = p_curr_size / 2.0F;
        auto quarter_size = half_size / 2.0F;
        auto new_center = glm::vec3(p_relative_position.x > 0 ? quarter_size : -quarter_size,
                                    p_relative_position.y > 0 ? quarter_size : -quarter_size,
                                    p_relative_position.z > 0 ? quarter_size : -quarter_size);
        auto new_relative_position = p_relative_position - new_center;
        return get_node(child, new_relative_position, half_size, p_depth < 0 ? -1 : p_depth - 1);
    }

    template <typename T1>
    void insert(Ref<OctTreeNode> p_curr, T1&& p_val, glm::vec3 p_relative_position, float p_curr_size,
                uint32_t p_depth) {
        if (p_depth == 0) {
            p_curr->value = std::forward<T1>(p_val);
            return;
        }
        auto subregion = get_subregion(p_relative_position);
        auto child = p_curr->children[subregion];
        if (child == MEM_NULL) {
            child = make_ref<OctTreeNode>(allocator);
            child->parent = p_curr;
            p_curr->children[subregion] = child;
        }
        auto half_size = p_curr_size / 2.0F;
        auto quarter_size = half_size / 2.0F;
        auto new_center = glm::vec3(p_relative_position.x > 0 ? quarter_size : -quarter_size,
                                    p_relative_position.y > 0 ? quarter_size : -quarter_size,
                                    p_relative_position.z > 0 ? quarter_size : -quarter_size);
        auto new_relative_position = p_relative_position - new_center;
        insert(child, std::forward<T1>(p_val), new_relative_position, half_size, p_depth - 1);
    }
};

} // namespace WhiteBirdEngine

#endif
