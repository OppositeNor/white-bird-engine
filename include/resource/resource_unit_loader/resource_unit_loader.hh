#ifndef __WBE_RESOURCE_UNIT_LOADER_HH__
#define __WBE_RESOURCE_UNIT_LOADER_HH__

#include "core/allocator/allocator.hh"
#include "core/allocator/stack_allocator.hh"
#include "core/parser/parser.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "utils/defs.hh"

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
