#ifndef __WBE_SCENE_MANAGER_H__
#define __WBE_SCENE_MANAGER_H__

#include "core/ecs/ecs.h"
#include "utils/interface/singleton.h"

namespace WhiteBirdEngine {

/**
 * @class SceneManager
 * @brief Manages all the scenes.
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
