#ifndef __WBE_HEAP_ALLOCATOR_ALIGNED_RAM_HH__
#define __WBE_HEAP_ALLOCATOR_ALIGNED_RAM_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/heap_allocator_aligned.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
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

    WBE_TRAIT_REQURIES(AllocatorTraitConcept);
};

/**
 * @class HeapAllocatorAlignedRAM
 * @brief Heap allocator pool with memory alignment support.
 *
 * @todo Test
 */
class HeapAllocatorAlignedRAM : public HeapAllocatorAligned {
public:

    HeapAllocatorAlignedRAM() {}
    virtual ~HeapAllocatorAlignedRAM() override {}
    HeapAllocatorAlignedRAM(const HeapAllocatorAlignedRAM&) = delete;
    HeapAllocatorAlignedRAM(HeapAllocatorAlignedRAM&&) = delete;
    HeapAllocatorAlignedRAM& operator=(const HeapAllocatorAlignedRAM&) = delete;
    HeapAllocatorAlignedRAM& operator=(HeapAllocatorAlignedRAM&&) = delete;

    virtual MemID allocate(size_t p_size, size_t p_alignment) override {
        size_t align_size = get_align_size(p_size, p_alignment);
        void* mem = aligned_alloc(p_alignment, align_size);
        MemID result = reinterpret_cast<MemID>(mem);
        allocated.insert(result);
        size_map[result] = align_size;
        return result;
    }

    virtual void deallocate(MemID p_mem) override {
        WBE_DEBUG_ASSERT(size_map.contains(p_mem));
        size_map.erase(p_mem);
        free(reinterpret_cast<void*>(p_mem));
    }

    virtual void* get(MemID p_id) const override {
        return reinterpret_cast<void*>(p_id);
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

    virtual void clear() override {
        allocated.clear();
    }

    virtual size_t get_allocated_data_size(MemID p_mem_id) const override {
        return size_map.at(p_mem_id);
    }

private:
    std::set<MemID> allocated;
    std::unordered_map<MemID, size_t> size_map;
};
}

#endif
