#ifndef __WBE_RESOURCE_UNIT_LOADER_H__
#define __WBE_RESOURCE_UNIT_LOADER_H__

#include "platform/file_system/path.h"
#include "resource/resource_chunk/resource_chunk.h"

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
     * @param p_res_key The resource key of the resource.
     * @param p_path The path of the resource.
     */
    virtual void load(ResourceChunk& p_chunk, const std::string& p_res_key, const Path& p_path) = 0;
};

}


#endif
