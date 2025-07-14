#ifndef __WBE_RU_LOADER_PPM_H__
#define __WBE_RU_LOADER_PPM_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
#include "resource/resource_unit_loader/ru_loader_ppm3.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderPPM
 * @brief Resource unit loader for ppm images.
 *
 */
class RULoaderPPM : public ResourceUnitLoader<RULoaderPPM> {
public:
    RULoaderPPM() = default;
    ~RULoaderPPM() {}
    RULoaderPPM(const RULoaderPPM&) = delete;
    RULoaderPPM(RULoaderPPM&&) = delete;
    RULoaderPPM& operator=(const RULoaderPPM&)= delete;
    RULoaderPPM& operator=(RULoaderPPM&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }

private:
    RULoaderPPM3 loader_ppm3;
};


}

#endif
