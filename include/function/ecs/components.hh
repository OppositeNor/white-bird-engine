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
#ifndef WBE_FILE_COMPONENTS_HH
#define WBE_FILE_COMPONENTS_HH

#include "core/allocator/heap_allocator_fixed_size_pool.hh"
#include "core/memory/reference_raw.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/function_utils.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "utils/utils.hh"
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string_view>
namespace WhiteBirdEngine {

template <typename T> using RefComp = RefRaw<T, HeapAllocatorFixedSizePool>;

/**
 * @class ComponentLabel
 * @brief Label of the entity.
 */
struct WBE_META(WBE_COMPONENT) ComponentLabel {
    static constexpr size_t MAX_NAME_SIZE = 15;
    // The buffer for the name.
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    Buffer<MAX_NAME_SIZE + 1> name_buffer;
    // The name of the component.
    std::string_view name;

    ComponentLabel() : name_buffer({.buffer = ""}), name(name_buffer.buffer) {}
};

/**
 * @class ComponentTransform
 * @brief Component that stores a transform based on scale, rotation and
 * translation data. The transform will be performed in the order of: scale,
 * rotate and then translation (i.e. translation * rotation * scale).
 */
struct WBE_META(WBE_COMPONENT) ComponentTransform {
    // The scale value: x, y, z
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 scale;
    // The rotation value in quaternion: r, x, y, z
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::quat rotation;
    // The translation value: x, y, z
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    glm::vec3 translation;
};

/**
 * @class ComponentTransform
 * @brief Transform component.
 */
struct WBE_META(WBE_COMPONENT) ComponentTransformMats {
    // The local transform matrix.
    glm::mat4 local_transform = glm::mat4(1);
    // The global transform matrix.
    glm::mat4 global_transform = glm::mat4(1);
    // A counter to check if this transform is up to date.
    int64_t version = -1;
};

/**
 * @class ComponentSceneTreeNode
 * @brief Contains information of a component inside a tree node.
 */
struct WBE_META(WBE_COMPONENT) ComponentSceneTreeNode {
    // The parent of this entity.
    EntityID parent = entt::null;
};

} // namespace WhiteBirdEngine
#endif
