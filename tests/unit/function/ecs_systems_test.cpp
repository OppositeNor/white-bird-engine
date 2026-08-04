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
#include "function/ecs/components.hh"
#include "function/ecs/systems.hh"
#include "function/function_utils.hh"
#include <entt/entt.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>

namespace WBE = WhiteBirdEngine;

namespace {

constexpr float EXPECTED_FLOAT_ERROR = 0.0001F;

WBE::ComponentTransform make_transform(glm::vec3 p_scale, glm::vec3 p_translation) {
    return WBE::ComponentTransform{
        .scale = p_scale,
        .rotation = glm::quat(1.0F, 0.0F, 0.0F, 0.0F),
        .translation = p_translation,
    };
}

glm::mat4 make_transform_mat(const WBE::ComponentTransform& p_transform) {
    glm::mat4 translation_mat = glm::translate(glm::mat4(1.0F), p_transform.translation);
    return glm::scale(translation_mat * glm::mat4_cast(p_transform.rotation), p_transform.scale);
}

void expect_mat_near(const glm::mat4& p_actual, const glm::mat4& p_expected) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_NEAR(p_actual[col][row], p_expected[col][row], EXPECTED_FLOAT_ERROR);
        }
    }
}

} // namespace

TEST(WBETransformSystemTest, EntityWithoutHierarchyIsTreatedAsRoot) {
    entt::registry registry;
    WBE::EntityID entity = registry.create();
    WBE::ComponentTransform transform = make_transform(glm::vec3(2.0F, 3.0F, 4.0F), glm::vec3(5.0F, 6.0F, 7.0F));

    registry.emplace<WBE::ComponentTransform>(entity, transform);
    registry.emplace<WBE::ComponentTransformMats>(entity);

    WBE::update_transform_mats(registry, 7);

    const auto& transform_mats = registry.get<WBE::ComponentTransformMats>(entity);
    glm::mat4 expected_transform = make_transform_mat(transform);
    expect_mat_near(transform_mats.local_transform, expected_transform);
    expect_mat_near(transform_mats.global_transform, expected_transform);
    EXPECT_EQ(transform_mats.version, 7);
}

TEST(WBETransformSystemTest, ChildUsesParentGlobalTransformFromHierarchyComponent) {
    entt::registry registry;
    WBE::EntityID parent = registry.create();
    WBE::EntityID child = registry.create();
    WBE::ComponentTransform parent_transform = make_transform(glm::vec3(1.0F), glm::vec3(10.0F, 0.0F, 0.0F));
    WBE::ComponentTransform child_transform = make_transform(glm::vec3(1.0F), glm::vec3(0.0F, 2.0F, 0.0F));

    registry.emplace<WBE::ComponentTransform>(parent, parent_transform);
    registry.emplace<WBE::ComponentTransformMats>(parent);
    registry.emplace<WBE::ComponentTransform>(child, child_transform);
    registry.emplace<WBE::ComponentTransformMats>(child);
    registry.emplace<WBE::ComponentHierarchy>(child, parent);

    WBE::update_transform_mats(registry, 3);

    const auto& parent_transform_mats = registry.get<WBE::ComponentTransformMats>(parent);
    const auto& child_transform_mats = registry.get<WBE::ComponentTransformMats>(child);
    glm::mat4 expected_parent_transform = make_transform_mat(parent_transform);
    glm::mat4 expected_child_local_transform = make_transform_mat(child_transform);
    expect_mat_near(parent_transform_mats.global_transform, expected_parent_transform);
    expect_mat_near(child_transform_mats.local_transform, expected_child_local_transform);
    expect_mat_near(child_transform_mats.global_transform, expected_parent_transform * expected_child_local_transform);
    EXPECT_EQ(parent_transform_mats.version, 3);
    EXPECT_EQ(child_transform_mats.version, 3);
}

TEST(WBETransformSystemTest, ChildWithMissingParentTransformIsTreatedAsRoot) {
    entt::registry registry;
    WBE::EntityID parent = registry.create();
    WBE::EntityID child = registry.create();
    WBE::ComponentTransform child_transform = make_transform(glm::vec3(3.0F), glm::vec3(0.0F, 4.0F, 0.0F));

    registry.emplace<WBE::ComponentTransform>(child, child_transform);
    registry.emplace<WBE::ComponentTransformMats>(child);
    registry.emplace<WBE::ComponentHierarchy>(child, parent);

    WBE::update_transform_mats(registry, 11);

    const auto& child_transform_mats = registry.get<WBE::ComponentTransformMats>(child);
    glm::mat4 expected_child_transform = make_transform_mat(child_transform);
    expect_mat_near(child_transform_mats.local_transform, expected_child_transform);
    expect_mat_near(child_transform_mats.global_transform, expected_child_transform);
    EXPECT_EQ(child_transform_mats.version, 11);
}

TEST(WBETransformSystemTest, EntityWithNullHierarchyParentIsTreatedAsRoot) {
    entt::registry registry;
    WBE::EntityID entity = registry.create();
    WBE::ComponentTransform transform = make_transform(glm::vec3(1.0F), glm::vec3(8.0F, 9.0F, 10.0F));

    registry.emplace<WBE::ComponentTransform>(entity, transform);
    registry.emplace<WBE::ComponentTransformMats>(entity);
    registry.emplace<WBE::ComponentHierarchy>(entity);

    WBE::update_transform_mats(registry, 13);

    const auto& transform_mats = registry.get<WBE::ComponentTransformMats>(entity);
    glm::mat4 expected_transform = make_transform_mat(transform);
    expect_mat_near(transform_mats.global_transform, expected_transform);
    EXPECT_EQ(transform_mats.version, 13);
}

TEST(WBETransformSystemTest, RecursivelyUpdatesMultiLevelHierarchyOutOfOrder) {
    entt::registry registry;
    WBE::EntityID grandparent = registry.create();
    WBE::EntityID parent = registry.create();
    WBE::EntityID child = registry.create();
    WBE::ComponentTransform grandparent_transform = make_transform(glm::vec3(1.0F), glm::vec3(1.0F, 0.0F, 0.0F));
    WBE::ComponentTransform parent_transform = make_transform(glm::vec3(1.0F), glm::vec3(0.0F, 2.0F, 0.0F));
    WBE::ComponentTransform child_transform = make_transform(glm::vec3(1.0F), glm::vec3(0.0F, 0.0F, 3.0F));

    registry.emplace<WBE::ComponentTransform>(child, child_transform);
    registry.emplace<WBE::ComponentTransformMats>(child);
    registry.emplace<WBE::ComponentHierarchy>(child, parent);
    registry.emplace<WBE::ComponentTransform>(parent, parent_transform);
    registry.emplace<WBE::ComponentTransformMats>(parent);
    registry.emplace<WBE::ComponentHierarchy>(parent, grandparent);
    registry.emplace<WBE::ComponentTransform>(grandparent, grandparent_transform);
    registry.emplace<WBE::ComponentTransformMats>(grandparent);

    WBE::update_transform_mats(registry, 17);

    const auto& grandparent_transform_mats = registry.get<WBE::ComponentTransformMats>(grandparent);
    const auto& parent_transform_mats = registry.get<WBE::ComponentTransformMats>(parent);
    const auto& child_transform_mats = registry.get<WBE::ComponentTransformMats>(child);
    glm::mat4 expected_grandparent_transform = make_transform_mat(grandparent_transform);
    glm::mat4 expected_parent_transform = make_transform_mat(parent_transform);
    glm::mat4 expected_child_transform = make_transform_mat(child_transform);
    expect_mat_near(grandparent_transform_mats.global_transform, expected_grandparent_transform);
    expect_mat_near(parent_transform_mats.global_transform, expected_grandparent_transform * expected_parent_transform);
    expect_mat_near(child_transform_mats.global_transform,
        expected_grandparent_transform * expected_parent_transform * expected_child_transform);
    EXPECT_EQ(grandparent_transform_mats.version, 17);
    EXPECT_EQ(parent_transform_mats.version, 17);
    EXPECT_EQ(child_transform_mats.version, 17);
}

TEST(WBECameraSystemTest, CameraViewUsesEntityGlobalTransformInverse) {
    entt::registry registry;
    WBE::EntityID entity = registry.create();
    WBE::ComponentTransform transform = make_transform(glm::vec3(1.0F), glm::vec3(3.0F, 4.0F, 5.0F));

    registry.emplace<WBE::ComponentTransform>(entity, transform);
    registry.emplace<WBE::ComponentTransformMats>(entity);
    registry.emplace<WBE::ComponentCamera>(entity);

    WBE::update_transform_mats(registry, 23);
    WBE::update_camera_views(registry);

    const auto& transform_mats = registry.get<WBE::ComponentTransformMats>(entity);
    const auto& camera = registry.get<WBE::ComponentCamera>(entity);
    expect_mat_near(camera.view, glm::inverse(transform_mats.global_transform));
}

TEST(WBECameraSystemTest, CameraLocalTransformParticipatesInViewMatrix) {
    entt::registry registry;
    WBE::EntityID entity = registry.create();
    WBE::ComponentTransform transform = make_transform(glm::vec3(1.0F), glm::vec3(2.0F, 0.0F, 0.0F));
    glm::mat4 camera_local_transform = glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, 1.5F, -2.0F));

    registry.emplace<WBE::ComponentTransform>(entity, transform);
    registry.emplace<WBE::ComponentTransformMats>(entity);
    registry.emplace<WBE::ComponentCamera>(entity, WBE::ComponentCamera{.local_transform = camera_local_transform});

    WBE::update_transform_mats(registry, 29);
    WBE::update_camera_views(registry);

    const auto& transform_mats = registry.get<WBE::ComponentTransformMats>(entity);
    const auto& camera = registry.get<WBE::ComponentCamera>(entity);
    expect_mat_near(camera.view, glm::inverse(transform_mats.global_transform * camera_local_transform));
}