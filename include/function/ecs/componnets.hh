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
#ifndef __WBE_COMPONENTS_HH__
#define __WBE_COMPONENTS_HH__

#include "core/allocator/heap_allocator_aligned_pool.hh"
#include "core/memory/reference_raw.hh"
#include "function/ecs/entity.hh"
#include "core/reflection/reflection_defs.hh"
#include "global/stl_allocator.hh"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
namespace WhiteBirdEngine {

using String = ::WhiteBirdEngine::string<HeapAllocatorAlignedPool>;

/**
 * @class ComponentLabel
 * @brief Label of the entity.
 */
WBE_STRUCT(ComponentLabel, WBE_COMPONENT) {
    // The name of the component.
    WBE_META(WBE_REFLECT)
    String name;
    // The parent of the entity.
    WBE_META(WBE_REFLECT)
    EntityID parent;
};

/**
 * @class ComponentTransform
 * @brief Transform component.
 */
WBE_STRUCT(ComponentTransform, WBE_COMPONENT) {
    // The position value, ordered x, y, z
    WBE_META(WBE_REFLECT)
    glm::vec3 position;
    // The rotation value in quaternion, ordered r, x, y, z
    WBE_META(WBE_REFLECT)
    glm::quat rotation;
    // The scale value in quaternion, ordered x, y, z
    WBE_META(WBE_REFLECT)
    glm::vec3 scale;

    // The local transform matrix.
    glm::mat4 local_transform;
    // The local inverse transform matrix.
    glm::mat4 local_inverse;
    // The global transform matrix.
    glm::mat4 global_transform;
    // The global inverse transform matrix.
    glm::mat4 global_inverse;
    // Parent.
    RefRaw<ComponentTransform> parent;
    // Tick tock checker for checking if the component is updated or not.
    bool tick_tock = false;
};

}

#endif
