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
#ifndef __WBE_SCENE_MANAGER_HH__
#define __WBE_SCENE_MANAGER_HH__

#include "core/ecs/ecs.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {

/**
 * @class SceneManager
 * @brief Manages scene create, change and destroy.
 *
 */
class SceneManager : public Singleton<SceneManager> {
public:
    SceneManager() = default;
    virtual ~SceneManager() {}

    /**
     * @brief Add an entity to the scene manager.
     *
     * @return The entity to add to the scene manager.
     */
    virtual EntityID add_entity() = 0;

    /**
     * @brief Remove an entity.
     *
     * @param p_entity_id The ID of the entity to be removed.
     */
    virtual void remove_entity(EntityID p_entity_id) = 0;


};

}

#endif
