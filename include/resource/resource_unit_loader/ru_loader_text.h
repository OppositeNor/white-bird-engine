#ifndef __WBE_RU_LOADER_TEXT_H__
#define __WBE_RU_LOADER_TEXT_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderText
 * @brief Resource unit loader for text resources.
 *
 */
class RULoaderText : public ResourceUnitLoader<RULoaderText> {
public:
    RULoaderText() = default;
    ~RULoaderText() {}
    RULoaderText(const RULoaderText&) = delete;
    RULoaderText(RULoaderText&&) = delete;
    RULoaderText& operator=(const RULoaderText&)= delete;
    RULoaderText& operator=(RULoaderText&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserDataType& p_parser_data) {
        return MEM_NULL; // TODO
    }

};


}

#endif
