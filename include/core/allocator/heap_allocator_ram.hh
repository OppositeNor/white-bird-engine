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
#ifndef __WBE_HEAP_ALLOCATOR_RAM_HH__
#define __WBE_HEAP_ALLOCATOR_RAM_HH__

#include "heap_allocator.hh"
#include "utils/defs.hh"
#include <unordered_map>
namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorRAM> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorRAM>);
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
    static constexpr bool IS_POOL = false;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_ALIGNABLE = false;
    static constexpr bool IS_LIMITED_SIZE = false;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
};

class HeapAllocatorRAM final : public HeapAllocator {
public:

    HeapAllocatorRAM() : curr_id(1) {}
    virtual ~HeapAllocatorRAM() override;

    virtual MemID allocate(size_t p_size) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) override {
        if (p_id == 0) {
            return nullptr;
        }
        return alloc_map.at(p_id);
    }

    virtual const void* get(MemID p_id) const override {
        if (p_id == 0) {
            return nullptr;
        }
        return alloc_map.at(p_id);
    }

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual const std::set<MemID>& get_allocated() const override {
        return allocated;
    }

    virtual bool is_empty() const override {
        return allocated.empty();
    }

    // TODO: Test
    virtual void clear() override {
        allocated.clear();
        alloc_map.clear();
    }

private:
    MemID curr_id;
    std::set<MemID> allocated;
    std::unordered_map<MemID, void*> alloc_map;

};

}

#endif
