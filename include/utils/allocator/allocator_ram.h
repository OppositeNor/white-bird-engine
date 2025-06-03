#ifndef __WBE_ALLOCATOR_RAM_H__
#define __WBE_ALLOCATOR_RAM_H__

#include "utils/allocator/allocator.h"
#include <unordered_map>
namespace WhiteBirdEngine {

class AllocatorRam : Allocator {
public:

    AllocatorRam() : curr_id(0) {}
    virtual ~AllocatorRam() override;

    virtual MemID allocate(size_t p_size) override;

    virtual void deallocate(MemID p_mem) override;

    virtual void* get(MemID p_id) override {
        return alloc_map.find(p_id)->second;
    }

    virtual uint32_t obj_count() const override {
        return allocated.size();
    }

    virtual const std::set<MemID>& get_allocated() const override {
        return allocated;
    }

private:
    MemID curr_id;
    std::set<MemID> allocated;
    std::unordered_map<MemID, void*> alloc_map;

};

}

#endif
