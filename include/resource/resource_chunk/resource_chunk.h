#ifndef __WBE_RESOURCE_CHUNK_H__
#define __WBE_RESOURCE_CHUNK_H__

#include "core/allocator/allocator.h"
#include "core/allocator/stack_allocator.h"
#include <cstddef>
#include <string>
#include <unordered_map>
namespace WhiteBirdEngine {

class ResourceManager;

class ResourceChunk {
public:
    ResourceChunk()
        : total_size(0) {}
    virtual ~ResourceChunk() {}
    ResourceChunk(const ResourceChunk&) = delete;
    ResourceChunk(ResourceChunk&&) = delete;
    ResourceChunk& operator=(const ResourceChunk&) = delete;
    ResourceChunk& operator=(ResourceChunk&&) = delete;

    virtual MemID get_resource(const std::string& p_key) const {
        return resource_map.at(p_key);
    }

    size_t get_size() const {
        return total_size;
    }
protected:

    template <typename T>
    MemID push_res(const std::string& p_res, StackAllocator& p_allocator) {
        total_size += sizeof(T);
        MemID result = p_allocator.allocate(sizeof(T));
        resource_map[p_res] = result;
        return result;
    }

private:
    size_t total_size;
    std::unordered_map<std::string, MemID> resource_map;
};

}

#endif
