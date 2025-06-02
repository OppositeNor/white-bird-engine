#ifndef __PE_ALLOCATOR_RAM_ATOMIC_H__
#define __PE_ALLOCATOR_RAM_ATOMIC_H__

#include "utils/allocator/pe_allocator.h"
#include <atomic>
namespace PencilEngine {

class AllocatorRamAtomic : Allocator {
public:
    AllocatorRamAtomic() {}
    virtual ~AllocatorRamAtomic() override;

    virtual void* allocate(size_t p_size) override;

    virtual void deallocate(void* p_mem) override;

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual const std::unordered_set<void*>& get_allocated() const override {
        return allocated;
    }

private:
    std::atomic<std::unordered_set<void*>> allocated;

};

}

#endif
