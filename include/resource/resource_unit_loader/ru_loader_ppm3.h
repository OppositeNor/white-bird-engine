#ifndef __WBE_RU_LOADER_PPM3_H__
#define __WBE_RU_LOADER_PPM3_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderPPM3
 * @brief Resource unit loader for ppm3 images.
 *
 */
class RULoaderPPM3 : public ResourceUnitLoader<RULoaderPPM3> {
public:
    RULoaderPPM3() = default;
    ~RULoaderPPM3() {}
    RULoaderPPM3(const RULoaderPPM3&) = delete;
    RULoaderPPM3(RULoaderPPM3&&) = delete;
    RULoaderPPM3& operator=(const RULoaderPPM3&)= delete;
    RULoaderPPM3& operator=(RULoaderPPM3&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserDataType& p_parser_data) {
        return MEM_NULL; // TODO
    }

};

}

#endif
