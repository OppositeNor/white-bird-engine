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
#ifndef WBE_FILE_SCENE_HH
#define WBE_FILE_SCENE_HH

#include "core/core_utils.hh"
#include <entt/entt.hpp>

namespace WhiteBirdEngine {

/**
 * @class Scene
 * @brief Scene class.
 */
class Scene {
public:
    Scene(HeapAllocatorDefault* p_allocator) : allocator(p_allocator) {}

    /**
     * @brief Get the allocator of the scene.
     *
     * @return The allocator of the scene.
     */
    HeapAllocatorDefault* get_allocator() { return allocator; }

    /**
     * @brief Get the allocator of the scene.
     *
     * @return The allocator of the scene.
     */
    const HeapAllocatorDefault* get_allocator() const { return allocator; }

private:
    HeapAllocatorDefault* allocator;
    entt::registry entities;
};

} // namespace WhiteBirdEngine

#endif
