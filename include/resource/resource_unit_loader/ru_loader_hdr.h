#ifndef __WBE_RU_LOADER_HDR_H__
#define __WBE_RU_LOADER_HDR_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
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

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserDataType& p_parser_data) {
        return MEM_NULL; // TODO
    }

};


}

#endif
