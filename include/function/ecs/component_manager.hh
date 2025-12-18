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
#ifndef WBE_FILE_COMPONENT_MANAGER_HH
#define WBE_FILE_COMPONENT_MANAGER_HH

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_fixed_size_pool.hh"
#include <bitset>
#include <cstdint>
#include <mutex>
#include <shared_mutex>

namespace WhiteBirdEngine {

/**
 * @brief Component manager.
 *
 * @todo Test.
 * @tparam T The type of the component.
 * @return The maximum number of components.
 */
template <typename T, size_t MaxComponents>
class ComponentManager {
public:
    ComponentManager()
        : allocator(sizeof(T), MaxComponents) {}

    /**
     * @brief The number of components waiting for freeing that will trigger a flush.
     */
    static constexpr uint32_t FLUSH_AFTER_FREE = 16;

    /**
     * @brief Called every certain period of time.
     */
    void update();

    /**
     * @brief Apply a system to the components.
     *
     * @tparam SysType The type of the system.
     * @param p_system The system to apply.
     */
    template <typename SysType>
    void apply(SysType& p_system) {
        std::shared_lock lock(mutex);
        p_system(allocator.get_mem_start(), allocator.obj_count(), wait_for_free);
    }

    /**
     * @brief Remove the components that are waiting for free.
     */
    void flush();

private:
    HeapAllocatorFixedSizePool allocator;
    std::bitset<HeapAllocatorFixedSizePool::MAX_OBJ> wait_for_free;
    std::bitset<HeapAllocatorFixedSizePool::MAX_OBJ> disabled;
    uint32_t num_wait_for_free = 0;
    mutable std::shared_mutex mutex;
};

template <typename T, size_t MaxComponents>
inline void ComponentManager<T, MaxComponents>::update() {
    // flushing
    if (num_wait_for_free > FLUSH_AFTER_FREE) {
        flush();
    }
}

template <typename T, size_t MaxComponents>
void ComponentManager<T, MaxComponents>::flush() {
    std::unique_lock lock(mutex);
    std::vector<MemID> objs = allocator.get_allocated();
    for (auto& obj : objs) {
        if (wait_for_free.test(obj)) {
            allocator.deallocate(obj);
        }
    }
    wait_for_free.reset();
    num_wait_for_free = 0;
}

} // namespace WhiteBirdEngine

#endif
