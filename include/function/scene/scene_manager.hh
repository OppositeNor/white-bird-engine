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
#ifndef WBE_FILE_SCENE_MANAGER_HH
#define WBE_FILE_SCENE_MANAGER_HH

#include "core/memory/reference_strong.hh"
#include "core/allocator/stl_allocator.hh"
#include "i_scene_manager.hh"
#include "scene.hh"
#include "utils/defs.hh"
#include <string>
namespace WhiteBirdEngine {

/**
 * @class SceneManager
 * @brief scene manager.
 *
 */
class SceneManager final : public ISceneManager {
public:
    WBE_R6_NDC_DELETE_COPY_MOVE_OVERRIDE(SceneManager)
    SceneManager(Map<std::string, Ref<Scene>> p_scene_map);

    virtual Ref<Scene> get_current_scene() override;

    virtual Ref<const Scene> get_current_scene() const override;

    virtual void change_scene(const std::string& p_scene_id) override;

private:
    Map<std::string, Ref<Scene>> scene_map;
};

} // namespace WhiteBirdEngine

#endif
