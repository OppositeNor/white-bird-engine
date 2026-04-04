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
#ifndef WBE_FILE_HEAP_ALLOCATOR_RAM_HH
#define WBE_FILE_HEAP_ALLOCATOR_RAM_HH

#include "core/allocator/i_allocator.hh"
#include "heap_allocator.hh"
#include "utils/defs.hh"
#include <cstddef>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
namespace WhiteBirdEngine {

template <>
struct AllocatorTrait<class HeapAllocatorRAM> final : public AllocatorTrait<HeapAllocator> {
    WBE_TRAIT(AllocatorTrait<HeapAllocatorRAM>);
    static constexpr bool IS_POOL = false;
    static constexpr bool IS_GURANTEED_CONTINUOUS = false;
    static constexpr bool IS_ALIGNABLE = false;
    static constexpr bool IS_LIMITED_SIZE = false;
    static constexpr bool IS_ALLOC_FIXED_SIZE = false;
    static constexpr bool IS_ATOMIC = false;
    static constexpr bool WILL_ADDR_MOVE = false;

    WBE_TRAIT_REQUIRES(AllocatorTraitConcept);
};

/**
 * @class HeapAllocatorRAM
 * @brief Allocator that uses standard RAM allocation (malloc/free).
 *
 */
class HeapAllocatorRAM final : public HeapAllocator {
public:
    HeapAllocatorRAM() = default;
    virtual ~HeapAllocatorRAM() override;
    WBE_R6_NDCD_DELETE_COPY_MOVE(HeapAllocatorRAM)

    virtual MemID allocate(size_t p_size) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) const override {
        if (p_id == 0) {
            return nullptr;
        }
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

    // TODO: Test
    virtual void clear() override {
        allocated.clear();
    }

    virtual operator std::string() const override {
        std::stringstream ss;
        ss << R"({"type":"HeapAllocatorRAM",)";
        ss << R"("obj_count":)" << obj_count() << ",";
        ss << R"("allocated":[)";
        bool first = true;
        for (const auto& mem_id : allocated) {
            if (!first) {
                ss << ",";
            }
            first = false;
            ss << mem_id;
        }
        ss << "]}";
        return ss.str();
    }

private:
    std::set<MemID> allocated;

};

} // namespace WhiteBirdEngine

#endif
