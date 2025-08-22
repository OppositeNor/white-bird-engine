#ifndef __WBE_RU_LOADER_HDR_HH__
#define __WBE_RU_LOADER_HDR_HH__

#include "resource/resource_unit_loader/resource_unit_loader.hh"
namespace WhiteBirdEngine {

/**
 * @class RULoaderHDR
 * @brief Resource unit loader for hdr images.
 *
 */
class RULoaderHDR : public ResourceUnitLoader<RULoaderHDR> {
public:
    RULoaderHDR() = default;
    ~RULoaderHDR() {}
    RULoaderHDR(const RULoaderHDR&) = delete;
    RULoaderHDR(RULoaderHDR&&) = delete;
    RULoaderHDR& operator=(const RULoaderHDR&)= delete;
    RULoaderHDR& operator=(RULoaderHDR&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }

};


}

#endif
