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
#include "utils/utils.hh"
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string_view>
namespace WhiteBirdEngine {

template <typename T>
using RefComp = RefRaw<T, HeapAllocatorFixedSizePool>;

/**
 * @class ComponentLabel
 * @brief Label of the entity.
 */
struct WBE_META(WBE_COMPONENT) ComponentLabel {
    static constexpr size_t MAX_NAME_SIZE = 15;
    // The buffer for the name.
    WBE_META(WBE_REFLECT)
    Buffer<MAX_NAME_SIZE + 1> name_buffer;
    // The name of the component.
    std::string_view name;

    ComponentLabel() : name_buffer({.buffer = ""}), name(name_buffer.buffer) {}
};

/**
 * @class ComponentLocalTransform
 * @brief Local transform component.
 */
struct WBE_META(WBE_COMPONENT) ComponentLocalTransform {
    // The position value, ordered x, y, z
    WBE_META(WBE_REFLECT)
    glm::vec3 position;
    // The rotation value in quaternion, ordered r, x, y, z
    WBE_META(WBE_REFLECT)
    glm::quat rotation;
    // The scale value in quaternion, ordered x, y, z
    WBE_META(WBE_REFLECT)
    glm::vec3 scale;
};

/**
 * @class ComponentTransform
 * @brief Transform component.
 */
struct WBE_META(WBE_COMPONENT) ComponentTransform {
    // The local transform matrix.
    glm::mat4 local_transform;
    // The global transform matrix.
    glm::mat4 global_transform;
    // The parent entity ID.
    EntityID parent_entity;
    // The number of children.
    EntityID children_count;
};

} // namespace WhiteBirdEngine
#endif
