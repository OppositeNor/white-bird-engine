#ifndef __WBE_RESOURCE_UNIT_LOADER_H__
#define __WBE_RESOURCE_UNIT_LOADER_H__

#include "core/allocator/allocator.h"
#include "core/allocator/stack_allocator.h"
#include "core/parser/parser.h"
#include "resource/resource_chunk/resource_chunk.h"
#include "utils/defs.h"

namespace WhiteBirdEngine {

/**
 * @class ResourceUnitLoader
 * @brief Resource unit loader.
 *
 */
template <typename ChildT>
class ResourceUnitLoader {
public:
    ResourceUnitLoader() = default;
    ~ResourceUnitLoader() {}

    /**
     * @brief Load a resource to a chunk.
     *
     * @param p_chunk The chunk to load resource to.
     * @param p_parser_data The parser data.
     */
    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        return static_cast<ChildT*>(this)->template load<ParserDataType>(p_chunk, p_parser_data, p_allocator);
    }
};

WBE_DECL_CRTP_CONCEPT(ResourceUnitLoader);

}


#endif
