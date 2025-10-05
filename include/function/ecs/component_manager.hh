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
#ifndef __WBE_COMPONENT_MANAGER_HH__
#define __WBE_COMPONENT_MANAGER_HH__

#include "core/allocator/heap_allocator_fixed_size_pool.hh"
#include <bitset>

namespace WhiteBirdEnigne {

/**
 * @brief Component manager.
 *
 * @todo Test.
 * @tparam T The type of the component.
 * @return The maximum number of components.
 */
template <typename T, size_t MAX_COMPONENTS>
class ComponentManager {
public:
    ComponentManager()
        : allocator(sizeof(T), MAX_COMPONENTS) {}

    /**
     * @brief Apply a system to the components.
     *
     * @tparam SysType The type of the system.
     * @param p_system The system to apply.
     */
    template <typename SysType>
    void apply(SysType& p_system) {
        p_system(allocator.get_mem_start(), allocator.obj_count(), enabled_components);
    }

    /**
     * @brief Remove the disabled components.
     */
    void flush();

private:
    HeapAllocatorFixedSizePool allocator;
    std::bitset<MAX_COMPONENTS> enabled_components;

};

template <typename T, size_t MAX_COMPONENTS>
void ComponentManager<T, MAX_COMPONENTS>::flush() {
    // TODO
}

}

#endif
