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
#ifndef WBE_FILE_SCENE_RESOURCE_UNIT_HH
#define WBE_FILE_SCENE_RESOURCE_UNIT_HH

#include "core/reflection/reflection_defs.hh"
#include "core/reflection/serializable.hh"
#include "resource/resource_unit/resource_unit.hh"

namespace WhiteBirdEngine {

/**
 * @class RUSchemeComponent
 * @brief Placeholder scheme for a serialized component resource.
 *
 * @todo Define the component type and value representation.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeComponent : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeComponent)
};

/**
 * @class RUSchemePrefab
 * @brief Placeholder scheme for a prefab resource.
 *
 * @todo Define node hierarchy and component references.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemePrefab : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemePrefab)
};

/**
 * @class RUSchemeWorld
 * @brief Placeholder scheme for a world resource.
 *
 * @todo Define render-graph, mesh, prefab, and component references.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeWorld : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeWorld)
};

/**
 * @class RUSchemeScene
 * @brief Placeholder scheme for a scene resource.
 *
 * @todo Define world references and initial active-world selection.
 */
struct WBE_META(WBE_SERIALIZABLE) RUSchemeScene : public RUScheme {
    WBE_DECL_SERIALIZABLE(RUSchemeScene)
};

/**
 * @class RUComponent
 * @brief Runtime marker for a component resource.
 *
 * @todo Define the runtime component representation.
 */
struct RUComponent : public ResourceUnit {};

/**
 * @class RUPrefab
 * @brief Runtime marker for a prefab resource.
 *
 * @todo Define the runtime prefab representation.
 */
struct RUPrefab : public ResourceUnit {};

/**
 * @class RUWorld
 * @brief Runtime marker for a world resource.
 *
 * @todo Define the runtime world representation.
 */
struct RUWorld : public ResourceUnit {};

/**
 * @class RUScene
 * @brief Runtime marker for a scene resource.
 *
 * @todo Define the runtime scene representation.
 */
struct RUScene : public ResourceUnit {};

} // namespace WhiteBirdEngine

#endif