#ifndef __WBE_ALLOCATOR_RAM_H__
#define __WBE_ALLOCATOR_RAM_H__

#include "utils/allocator/allocator.h"
namespace WhiteBirdEngine {

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
