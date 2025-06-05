#ifndef __WBE_ALLOCATOR_FIXED_SIZE_POOL_H__
#define __WBE_ALLOCATOR_FIXED_SIZE_POOL_H__

#include "utils/allocator/allocator.h"
#include "utils/defs.h"
#include <cstring>
#include <unordered_map>

namespace WhiteBirdEngine {

template <size_t SIZE>
class AllocatorFixedSizePool final : public Allocator {
public:
    static constexpr uint32_t MAX_OBJ_COUNT = 128;

    AllocatorFixedSizePool()
        : AllocatorFixedSizePool(64) {}
    virtual ~AllocatorFixedSizePool() override;
    AllocatorFixedSizePool(const AllocatorFixedSizePool&) = delete;
    AllocatorFixedSizePool(AllocatorFixedSizePool&&) = delete;
    AllocatorFixedSizePool& operator=(const AllocatorFixedSizePool&) = delete;
    AllocatorFixedSizePool& operator=(AllocatorFixedSizePool&&) = delete;

    AllocatorFixedSizePool(uint32_t p_max_obj)
        : curr_id(1), max_obj(p_max_obj), alloc_obj_count(0),
          mem_chunk(std::make_unique<std::byte[]>(p_max_obj * SIZE)) {
        WBE_DEBUG_ASSERT(p_max_obj < MAX_OBJ_COUNT);
    }

    virtual MemID allocate(size_t p_size = SIZE) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) override {
        if (p_id == 0) {
            return nullptr;
        }
        return alloc_map[p_id];
    }

    virtual uint32_t obj_count() const override {
        return alloc_obj_count;
    }

    virtual const std::set<MemID>& get_allocated() const override {
        return allocated;
    }

    virtual operator std::string() const override;

    const void* get_mem_start() const {
        return mem_chunk.get();
    }

private:
    MemID curr_id;
    uint32_t max_obj;
    uint32_t alloc_obj_count;
    std::unique_ptr<std::byte[]> mem_chunk;
    std::unordered_map<MemID, void*> alloc_map;
    std::unordered_map<void*, MemID> alloc_map_reverse;
    std::set<MemID> allocated;

    void update_map_value(MemID p_id, void* p_addr) {
        alloc_map[p_id] = p_addr;
        alloc_map_reverse[p_addr] = p_id;
    }
};

template <size_t SIZE>
struct AllocatorTrait<AllocatorFixedSizePool<SIZE>> {
    WBE_TRAIT(AllocatorTrait<AllocatorFixedSizePool<SIZE>>);
    static constexpr bool IS_POOL = true;
    static constexpr bool IS_GURANTEED_CONTINUOUS = true;
    static constexpr bool IS_LIMITED_SIZE = true;
    static constexpr bool IS_ALLOC_FIXED_SIZE = true;
    static constexpr bool IS_ATOMIC = false;

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
};

template <size_t SIZE>
AllocatorFixedSizePool<SIZE>::~AllocatorFixedSizePool() {
    if (obj_count() != 0) {
        // TODO: warning when not empty
    }
}

template <size_t SIZE>
inline MemID AllocatorFixedSizePool<SIZE>::allocate(size_t p_size) {
    WBE_DEBUG_ASSERT(p_size == SIZE);
    if (alloc_obj_count >= max_obj) {
        throw std::runtime_error("Failed to allocate memory: not enough space for memory pool.");
    }
    void* start_pos = mem_chunk.get() + alloc_obj_count * SIZE;
    update_map_value(curr_id, start_pos);
    ++alloc_obj_count;
    return curr_id++;
}

template <size_t SIZE>
inline void AllocatorFixedSizePool<SIZE>::deallocate(MemID p_mem) {
    auto addr_iter = alloc_map.find(p_mem);
    if (addr_iter == alloc_map.end()) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated in this memory pool.");
    }
    void* copy_from = mem_chunk.get() + (alloc_obj_count - 1) * SIZE;
    void* copy_to = addr_iter->second;
    memcpy(copy_to, copy_from, SIZE);
    update_map_value(alloc_map_reverse[copy_from], copy_to);
    alloc_map_reverse.erase(copy_from);
    --alloc_obj_count;
    alloc_map.erase(p_mem);
    allocated.erase(p_mem);
}

template <size_t SIZE>
inline AllocatorFixedSizePool<SIZE>::operator std::string() const {
    std::stringstream ss;
    ss << "AllocatorFix: {obj_count: " << obj_count() << ";\n" << "max_obj: " << max_obj << "\n};";
    return ss.str();
}

}

#endif
