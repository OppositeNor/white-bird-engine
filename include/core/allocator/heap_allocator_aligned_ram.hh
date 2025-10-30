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
#ifndef __WBE_HEAP_ALLOCATOR_ALIGNED_RAM_HH__
#define __WBE_HEAP_ALLOCATOR_ALIGNED_RAM_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include "core/logging/log.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <bit>
#include <set>
#include <unordered_map>
namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorAlignedRAM> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorAlignedRAM>);
    static constexpr AllocatorType TYPE = AllocatorType::HEAP_ALLOCATOR;
    static constexpr bool IS_POOL = false;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_ALIGNABLE = true;
    static constexpr bool IS_LIMITED_SIZE = false;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};

/**
 * @class HeapAllocatorAlignedRAM
 * @brief Heap allocator pool with memory alignment support.
 *
 * @todo Test
 */
class HeapAllocatorAlignedRAM final : public HeapAllocatorAligned {
public:

    HeapAllocatorAlignedRAM() {}
    virtual ~HeapAllocatorAlignedRAM() override {
        if (!allocated.empty()) {
            wbe_console_log(WBE_CHANNEL_GLOBAL)->warning("Non-empty allocator destructed.");
        }
        for (auto data : allocated) {
            free(reinterpret_cast<void*>(data));
        }
    }
    HeapAllocatorAlignedRAM(const HeapAllocatorAlignedRAM&) = delete;
    HeapAllocatorAlignedRAM(HeapAllocatorAlignedRAM&&) = delete;
    HeapAllocatorAlignedRAM& operator=(const HeapAllocatorAlignedRAM&) = delete;
    HeapAllocatorAlignedRAM& operator=(HeapAllocatorAlignedRAM&&) = delete;

    virtual MemID allocate(size_t p_size, size_t p_alignment = WBE_DEFAULT_ALIGNMENT) override {
        if (p_size == 0) {
            return MEM_NULL;
        }
        size_t align_size = get_align_size(p_size, p_alignment);
        void* mem = aligned_alloc(p_alignment, align_size);
        MemID result = std::bit_cast<MemID>(mem);
        allocated.insert(result);
        size_map[result] = align_size;
        return result;
    }

    virtual void deallocate(MemID p_mem) override {
        WBE_DEBUG_ASSERT(size_map.contains(p_mem));
        size_map.erase(p_mem);
        allocated.erase(p_mem);
        free(std::bit_cast<void*>(p_mem));
    }

    virtual const void* get(MemID p_id) const override {
        WBE_DEBUG_ASSERT(allocated.find(p_id) != allocated.end());
        return reinterpret_cast<void*>(p_id);
    }

    virtual void* get(MemID p_id) override {
        WBE_DEBUG_ASSERT(allocated.find(p_id) != allocated.end());
        return reinterpret_cast<void*>(p_id);
    }

    uint32_t obj_count() const {
        return allocated.size();
    }

    std::set<MemID> get_allocated() const {
        return allocated;
    }

    virtual bool is_empty() const override {
        return allocated.empty();
    }

    virtual void clear() override {
        allocated.clear();
    }

    virtual size_t get_allocated_data_size(MemID p_mem_id) const override {
        return size_map.at(p_mem_id);
    }

    virtual operator std::string() const override {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"HeapAllocatorAlignedRAM\",";
        ss << "\"obj_count\":" << obj_count() << ",";
        ss << "\"allocated\":[";
        bool first = true;
        for (auto& mem_id : allocated) {
            if (!first) ss << ",";
            first = false;
            ss << "{"
               << "\"mem_id\":" << mem_id << ","
               << "\"size\":" << size_map.at(mem_id)
               << "}";
        }
        ss << "]";
        ss << "}";
        return ss.str();
    }

private:
    std::set<MemID> allocated;
    std::unordered_map<MemID, size_t> size_map;
};
}

#endif
