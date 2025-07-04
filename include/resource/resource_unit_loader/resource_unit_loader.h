#ifndef __WBE_RESOURCE_UNIT_LOADER_H__
#define __WBE_RESOURCE_UNIT_LOADER_H__

#include "core/allocator/allocator.h"
#include "platform/file_system/path.h"

namespace WhiteBirdEngine {

/**
 * @class ResourceUnitLoader
 * @brief Resource unit loader.
 *
 */
class ResourceUnitLoader {
public:
    ResourceUnitLoader() = default;
    virtual ~ResourceUnitLoader() {}

    /**
     * @brief Load a resource to a chunk.
     *
     * @param p_chunk The chunk to load resource to.
     * @param p_path The path of the resource.
     */
    virtual MemID load(class ResourceChunk& p_chunk, const Path& p_path) = 0;
};

}


#endif
