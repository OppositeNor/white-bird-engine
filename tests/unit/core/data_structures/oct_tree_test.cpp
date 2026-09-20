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
#include "core/allocator/heap_allocator_ram.hh"
#include "core/data_structures/oct_tree.hh"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <utility>

using WhiteBirdEngine::HeapAllocatorRAM;
using WhiteBirdEngine::OctTree;

TEST(WBEOctTreeTest, EmptyTreeHasRootAndReleasesNodes) {
    HeapAllocatorRAM allocator;
    {
        OctTree<int> tree(&allocator, {10.0F, 20.0F, 30.0F}, 8.0F);
        auto root = tree.get_node({10.0F, 20.0F, 30.0F});
        ASSERT_NE(root, nullptr);
        EXPECT_FALSE(root->value.has_value());
        EXPECT_FALSE(root->parent.is_valid());
        root = nullptr;
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEOctTreeTest, RootValueCanBeReplacedWithoutRemovingChildren) {
    HeapAllocatorRAM allocator;
    OctTree<int> tree(&allocator, {0.0F, 0.0F, 0.0F}, 8.0F);
    tree.insert(7, {0.0F, 0.0F, 0.0F}, 0);
    tree.insert(11, {1.0F, 1.0F, 1.0F}, 1);
    tree.insert(9, {0.0F, 0.0F, 0.0F}, 0);

    auto root = tree.get_node({1.0F, 1.0F, 1.0F}, 0);
    ASSERT_TRUE(root->value.has_value());
    EXPECT_EQ(*root->value, 9);
    EXPECT_EQ(*tree.get_node({1.0F, 1.0F, 1.0F})->value, 11);
}

TEST(WBEOctTreeTest, DistinguishesAllEightOctantsAndLinksParents) {
    HeapAllocatorRAM allocator;
    OctTree<int> tree(&allocator, {10.0F, 20.0F, 30.0F}, 8.0F);
    const std::array<glm::vec3, 8> positions = {
        glm::vec3{11.0F, 21.0F, 31.0F}, glm::vec3{11.0F, 21.0F, 29.0F},
        glm::vec3{11.0F, 19.0F, 31.0F}, glm::vec3{11.0F, 19.0F, 29.0F},
        glm::vec3{9.0F, 21.0F, 31.0F},  glm::vec3{9.0F, 21.0F, 29.0F},
        glm::vec3{9.0F, 19.0F, 31.0F},  glm::vec3{9.0F, 19.0F, 29.0F},
    };
    for (size_t i = 0; i < positions.size(); ++i) {
        tree.insert(static_cast<int>(i), positions[i], 1);
    }

    auto root = tree.get_node(positions[0], 0);
    for (size_t i = 0; i < positions.size(); ++i) {
        auto node = tree.get_node(positions[i]);
        ASSERT_NE(node, nullptr);
        ASSERT_TRUE(node->value.has_value());
        EXPECT_EQ(*node->value, i);
        EXPECT_EQ(node.get(), root->children[i].get());
        EXPECT_EQ(node->parent.lock().get(), root.get());
    }
}

TEST(WBEOctTreeTest, TraversesRelativeChildCentersAndReturnsExistingAncestor) {
    HeapAllocatorRAM allocator;
    OctTree<int> tree(&allocator, {10.0F, 20.0F, 30.0F}, 8.0F);
    tree.insert(42, {11.0F, 21.0F, 31.0F}, 2);

    auto root = tree.get_node({11.0F, 21.0F, 31.0F}, 0);
    auto first_level = tree.get_node({11.0F, 21.0F, 31.0F}, 1);
    auto second_level = tree.get_node({11.0F, 21.0F, 31.0F}, 2);
    EXPECT_FALSE(first_level->value.has_value());
    ASSERT_TRUE(second_level->value.has_value());
    EXPECT_EQ(*second_level->value, 42);
    EXPECT_EQ(first_level->parent.lock().get(), root.get());
    EXPECT_EQ(second_level->parent.lock().get(), first_level.get());
    EXPECT_EQ(tree.get_node({11.0F, 21.0F, 31.0F}, 9).get(), second_level.get());
    EXPECT_EQ(tree.get_node({9.0F, 21.0F, 31.0F}).get(), root.get());
}

TEST(WBEOctTreeTest, CenterPlanesBelongToNonpositiveOctants) {
    HeapAllocatorRAM allocator;
    OctTree<int> tree(&allocator, {0.0F, 0.0F, 0.0F}, 8.0F);
    tree.insert(1, {0.0F, 0.0F, 0.0F}, 1);
    tree.insert(2, {1.0F, 1.0F, 1.0F}, 1);

    auto root = tree.get_node({0.0F, 0.0F, 0.0F}, 0);
    EXPECT_EQ(*root->children[OctTree<int>::OctTreeNode::NX_NY_NZ]->value, 1);
    EXPECT_EQ(*root->children[OctTree<int>::OctTreeNode::PX_PY_PZ]->value, 2);
}

TEST(WBEOctTreeTest, MovingTreeTransfersNodeOwnership) {
    HeapAllocatorRAM allocator;
    {
        OctTree<int> source(&allocator, {0.0F, 0.0F, 0.0F}, 8.0F);
        source.insert(5, {1.0F, 1.0F, 1.0F}, 1);
        OctTree<int> destination(std::move(source));
        EXPECT_EQ(*destination.get_node({1.0F, 1.0F, 1.0F})->value, 5);
    }
    EXPECT_TRUE(allocator.is_empty());
}

TEST(WBEOctTreeTest, RejectsInvalidConstruction) {
    HeapAllocatorRAM allocator;
    EXPECT_THROW((OctTree<int>(nullptr, {0.0F, 0.0F, 0.0F}, 8.0F)), std::runtime_error);
    EXPECT_THROW((OctTree<int>(&allocator, {0.0F, 0.0F, 0.0F}, 0.0F)), std::runtime_error);
    EXPECT_THROW((OctTree<int>(&allocator, {0.0F, 0.0F, 0.0F}, -1.0F)), std::runtime_error);
    EXPECT_THROW((OctTree<int>(&allocator, {std::numeric_limits<float>::infinity(), 0.0F, 0.0F}, 8.0F)),
                 std::runtime_error);
}

TEST(WBEOctTreeTest, RejectsPositionsOutsideRootAndUnrepresentableDepth) {
    HeapAllocatorRAM allocator;
    OctTree<int> tree(&allocator, {0.0F, 0.0F, 0.0F}, 8.0F);
    EXPECT_THROW(tree.insert(1, {4.1F, 0.0F, 0.0F}, 1), std::runtime_error);
    EXPECT_THROW(tree.get_node({0.0F, 0.0F, -4.1F}), std::runtime_error);
    EXPECT_THROW(tree.insert(1, {0.0F, 0.0F, 0.0F}, UINT32_MAX), std::runtime_error);
    EXPECT_FALSE(tree.get_node({0.0F, 0.0F, 0.0F})->value.has_value());
}
