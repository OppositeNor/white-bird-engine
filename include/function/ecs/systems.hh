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
#ifndef __WBE_SYSTEMS_HH__
#define __WBE_SYSTEMS_HH__

#include "function/ecs/componnets.hh"
#include "glm/ext/matrix_transform.hpp"
#include <bitset>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace WhiteBirdEngine {

/**
 * @brief Update the local transform of components.
 *
 * @todo Test.
 * @param p_transforms The transforms to update.
 * @param p_num The number of transforms to be updated.
 * @param p_enabled_components The enabled components.
 */
template <size_t MAX_COMPONENTS>
inline void update_local_transform(ComponentTransform* p_transforms, size_t p_num, std::bitset<MAX_COMPONENTS> p_enabled_components) {
    for (size_t i = 0; i < p_num; ++i) {
        if (p_enabled_components.test(i)) {
            auto& transform = p_transforms[i];
            transform.local_transform =
                glm::translate(glm::scale(glm::mat4(), transform.position)
                               * glm::mat4_cast(transform.rotation), transform.position);
        }
    }
}

/**
 * @brief Update the glboal transform of componetns.
 *
 * @todo Test.
 * @param p_transforms The tnrasform components.
 * @param p_num The upper bound of transfrom components.
 * @param p_enabled_components The enabled components.
 */
template <size_t MAX_COMPONENTS>
struct UpdateGlobalTransform {
    /**
     * @brief Used for keeping track if a component has been updated or not altered every time when called.
     */
    bool tick_tock = true;
    void operator()(ComponentTransform* p_transforms, size_t p_num, std::bitset<MAX_COMPONENTS> p_enabled_components) {
        for (size_t i = 0; i < p_num; ++i) {
            if (p_enabled_components.test(i)) {
                auto& transform = p_transforms[i];
                if (transform.tick_tock == tick_tock) {
                    continue;
                }
                update_transform(transform);
            }
        }
        tick_tock = !tick_tock;
    }

    void update_transform(ComponentTransform& p_transform) {
        if (tick_tock == p_transform.tick_tock) {
            return;
        }
        if (p_transform.parent == nullptr) {
            p_transform.tick_tock = tick_tock;
            p_transform.global_transform = p_transform.local_transform;
            p_transform.global_inverse = p_transform.local_inverse;
            return;
        }
        update_transform(*(p_transform.parent));
        p_transform.global_transform = p_transform.parent->global_transform * p_transform.local_transform;
        p_transform.global_inverse = p_transform.local_inverse * p_transform.global_inverse;
    }
};

/**
 * @brief Update the glboal transform of componetns.
 *
 * @todo Test.
 * @param p_transforms The tnrasform components.
 * @param p_num The upper bound of transfrom components.
 * @param p_enabled_components The enabled components.
 */
template <size_t MAX_COMPONENTS>
inline UpdateGlobalTransform<MAX_COMPONENTS> update_global_transform;

}


#endif
