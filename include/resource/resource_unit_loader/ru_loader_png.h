#ifndef __WBE_RU_LOADER_PNG_H__
#define __WBE_RU_LOADER_PNG_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderPNG
 * @brief Resource unit loader for png images.
 *
 */
class RULoaderPNG : public ResourceUnitLoader<RULoaderPNG> {
public:
    RULoaderPNG() = default;
    ~RULoaderPNG() {}
    RULoaderPNG(const RULoaderPNG&) = delete;
    RULoaderPNG(RULoaderPNG&&) = delete;
    RULoaderPNG& operator=(const RULoaderPNG&)= delete;
    RULoaderPNG& operator=(RULoaderPNG&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserDataType& p_parser_data) {
        return MEM_NULL; // TODO
    }

};


}

#endif
