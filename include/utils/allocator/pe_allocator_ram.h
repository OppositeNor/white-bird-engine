#ifndef __PE_ALLOCATOR_RAM_H__
#define __PE_ALLOCATOR_RAM_H__

#include "utils/allocator/pe_allocator.h"
namespace PencilEngine {

class AllocatorRam : Allocator {
public:

    AllocatorRam() {}
    virtual ~AllocatorRam() override;

    virtual void* allocate(size_t p_size) override;

    virtual void deallocate(void* p_mem) override;

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual const std::unordered_set<void*>& get_allocated() const override {
        return allocated;
    }

private:
    std::unordered_set<void*> allocated;

};

}

#endif
