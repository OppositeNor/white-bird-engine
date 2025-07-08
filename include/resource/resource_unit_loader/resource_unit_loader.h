#ifndef __WBE_RESOURCE_UNIT_LOADER_H__
#define __WBE_RESOURCE_UNIT_LOADER_H__

#include "core/allocator/allocator.h"
#include "core/parser/parser.h"

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
    template <ParserDataConcept ParserDataType>
    MemID load(class ResourceChunk& p_chunk, const ParserDataType& p_parser_data) {
        return static_cast<ChildT*>(this)->template load<ParserDataType>(p_chunk, p_parser_data);
    }
};

}


#endif
