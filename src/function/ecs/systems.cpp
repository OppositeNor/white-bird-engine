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
#include "function/ecs/systems.hh"
#include "function/ecs/components.hh"

namespace WhiteBirdEngine {

static void update_global_transform(entt::registry& p_registry,
                                    const ComponentSceneTreeNode& p_parent_info,
                                    ComponentTransformMats& p_transform_mats,
                                    int64_t p_version) {
    if (p_transform_mats.version == p_version) {
        // Transform up to date, return.
        return;
    }
    if (// No parent, update global transform to local transform.
        p_parent_info.parent == entt::null
        // Parent invalid. Probably because parent does not have a transform component, or it is removed.
        // In both cases, only update global transform to local transform.
        || !p_registry.all_of<const ComponentSceneTreeNode, ComponentTransformMats>(p_parent_info.parent)) {
        p_transform_mats.global_transform = p_transform_mats.local_transform;
        return;
    }
    // Get parent info.
    auto [pparent_info, ptransform_mats] = p_registry.get<const ComponentSceneTreeNode, ComponentTransformMats>(p_parent_info.parent);
    if (ptransform_mats.version != p_version) {
        update_global_transform(p_registry, pparent_info, ptransform_mats, p_version);
    }
    p_transform_mats.global_transform = ptransform_mats.global_transform * p_transform_mats.local_transform;
    // Update the version to newest.
    p_transform_mats.version = p_version;
}

void update_transform_mats(entt::registry& p_registry, int64_t p_version) {
    // Update local transforms.
    auto local_transform_iterate_view = p_registry.view<const ComponentTransform, ComponentTransformMats>();
    for (auto [entity, transform, transform_mats] : local_transform_iterate_view.each()) {
        // Note that, for some reason, GLM decides to perform the transform BEFORE the transform passed
        // in (i.e. when declaring glm::translate(M, T)), we are actually doing M(T(v)) but not T(M(v)).
        // So the order: scale, then rotate, then translate, we have to write it
        // backwards (translate(rotate(scale))).
        auto translation_mat = glm::translate(glm::mat4(1.0F), transform.translation);
        auto t_times_r_mat = translation_mat * glm::mat4_cast(transform.rotation);
        transform_mats.local_transform = glm::scale(t_times_r_mat, transform.scale);
    }

    // Update global transforms.
    auto global_transform_iterate_view = p_registry.view<const ComponentSceneTreeNode, ComponentTransformMats>();
    for (auto [entity, parent_info, transform_mats] : global_transform_iterate_view.each()) {
        update_global_transform(p_registry, parent_info, transform_mats, p_version);
    }
}

} // namespace WhiteBirdEngine

