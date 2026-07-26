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
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/scene/scene.hh"
#include "function/scene/scene_node.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <entt/entt.hpp>
#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>

namespace WBE = WhiteBirdEngine;

namespace {

template <typename T>
WBE::TypeID get_component_type_id() {
    return entt::type_hash<T>::value();
}

WBE::ComponentTransform make_transform() {
    return WBE::ComponentTransform{
        .scale = glm::vec3(1.0F, 2.0F, 3.0F),
        .rotation = glm::quat(1.0F, 0.0F, 0.0F, 0.0F),
        .translation = glm::vec3(4.0F, 5.0F, 6.0F),
    };
}

class WBESceneTest : public ::testing::Test {
protected:
    void SetUp() override {
        global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    }

    void TearDown() override {
        global.reset();
    }

    WBE::HeapAllocatorDefault* get_allocator() {
        return global->engine_core->pool_allocator;
    }

    std::unique_ptr<WBE::Global> global;
};

} // namespace

TEST_F(WBESceneTest, SceneExposesAllocatorAndEntityRegistry) {
    WBE::Scene scene(get_allocator());

    WBE::EntityID entity = scene.get_entities().create();

    EXPECT_EQ(scene.get_allocator(), get_allocator());
    EXPECT_TRUE(scene.get_entities().valid(entity));
}

TEST_F(WBESceneTest, SceneCachesComponentListsByComponentTypeId) {
    WBE::Scene scene(get_allocator());
    WBE::EntityID entity = scene.get_entities().create();
    const WBE::TypeID transform_type_id = get_component_type_id<WBE::ComponentTransform>();

    EXPECT_EQ(scene.get_component_list(transform_type_id), nullptr);

    scene.get_entities().emplace<WBE::ComponentTransform>(entity, make_transform());
    entt::sparse_set* component_list = scene.get_component_list(transform_type_id);

    ASSERT_NE(component_list, nullptr);
    EXPECT_TRUE(component_list->contains(entity));
    EXPECT_EQ(scene.get_component_list(transform_type_id), component_list);
}

TEST_F(WBESceneTest, SceneReturnsComponentTypeIdsAttachedToEntity) {
    WBE::Scene scene(get_allocator());
    WBE::EntityID first_entity = scene.get_entities().create();
    WBE::EntityID second_entity = scene.get_entities().create();
    const WBE::TypeID transform_type_id = get_component_type_id<WBE::ComponentTransform>();
    const WBE::TypeID label_type_id = get_component_type_id<WBE::ComponentLabel>();

    scene.get_entities().emplace<WBE::ComponentTransform>(first_entity, make_transform());
    scene.get_entities().emplace<WBE::ComponentLabel>(first_entity);
    scene.get_entities().emplace<WBE::ComponentLabel>(second_entity);

    WBE::Vector<WBE::TypeID> first_component_type_ids = scene.get_component_type_ids(first_entity);
    WBE::Vector<WBE::TypeID> second_component_type_ids = scene.get_component_type_ids(second_entity);

    EXPECT_NE(std::find(first_component_type_ids.begin(), first_component_type_ids.end(), transform_type_id),
        first_component_type_ids.end());
    EXPECT_NE(std::find(first_component_type_ids.begin(), first_component_type_ids.end(), label_type_id),
        first_component_type_ids.end());
    EXPECT_EQ(std::find(second_component_type_ids.begin(), second_component_type_ids.end(), transform_type_id),
        second_component_type_ids.end());
    EXPECT_NE(std::find(second_component_type_ids.begin(), second_component_type_ids.end(), label_type_id),
        second_component_type_ids.end());
}

TEST_F(WBESceneTest, SceneNodeCreatesAndDestroysEntityInScene) {
    auto scene = WBE::make_ref<WBE::Scene>(get_allocator(), get_allocator());
    WBE::EntityID entity = entt::null;

    {
        auto node = WBE::make_ref<WBE::SceneNode>(get_allocator(), scene);
        entity = node->get_entity();

        EXPECT_TRUE(scene->get_entities().valid(entity));
        EXPECT_TRUE(scene->get_scene_node(entity) == node);
    }

    EXPECT_FALSE(scene->get_entities().valid(entity));
    EXPECT_TRUE(scene->get_scene_node(entity).is_null());
}

TEST_F(WBESceneTest, SceneNodeUsesSceneComponentListsForComponentState) {
    auto scene = WBE::make_ref<WBE::Scene>(get_allocator(), get_allocator());
    auto node = WBE::make_ref<WBE::SceneNode>(get_allocator(), scene);
    const WBE::TypeID transform_type_id = get_component_type_id<WBE::ComponentTransform>();

    EXPECT_FALSE(node->has_component(transform_type_id));

    scene->get_entities().emplace<WBE::ComponentTransform>(node->get_entity(), make_transform());

    EXPECT_TRUE(node->has_component(transform_type_id));
    EXPECT_NE(scene->get_component_list(transform_type_id), nullptr);
    WBE::Vector<WBE::TypeID> component_type_ids = node->get_component_type_ids();
    EXPECT_NE(std::find(component_type_ids.begin(), component_type_ids.end(), transform_type_id), component_type_ids.end());

    node->remove_component(transform_type_id);

    EXPECT_FALSE(node->has_component(transform_type_id));
    EXPECT_FALSE(scene->get_entities().all_of<WBE::ComponentTransform>(node->get_entity()));
    EXPECT_THROW(node->remove_component(transform_type_id), std::runtime_error);
}

TEST_F(WBESceneTest, SceneNodeHierarchyIsTrackedByComponent) {
    auto scene = WBE::make_ref<WBE::Scene>(get_allocator(), get_allocator());
    auto parent = WBE::make_ref<WBE::SceneNode>(get_allocator(), scene);
    auto child = WBE::make_ref<WBE::SceneNode>(get_allocator(), scene);
    auto next_parent = WBE::make_ref<WBE::SceneNode>(get_allocator(), scene);

    WBE::ComponentHierarchy& hierarchy = scene->get_entities().emplace<WBE::ComponentHierarchy>(child->get_entity());
    hierarchy.parent = parent->get_entity();

    EXPECT_TRUE(scene->get_scene_node(parent->get_entity()) == parent);
    EXPECT_TRUE(scene->get_scene_node(child->get_entity()) == child);
    EXPECT_TRUE(hierarchy.parent == parent->get_entity());

    hierarchy.parent = next_parent->get_entity();

    EXPECT_TRUE(hierarchy.parent == next_parent->get_entity());
}