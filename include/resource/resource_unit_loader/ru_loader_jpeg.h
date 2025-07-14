#ifndef __WBE_RU_LOADER_JPEG_H__
#define __WBE_RU_LOADER_JPEG_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderJPEG
 * @brief Resource unit loader for jpeg images.
 *
 */
class RULoaderJPEG : public ResourceUnitLoader<RULoaderJPEG> {
public:
    RULoaderJPEG() = default;
    ~RULoaderJPEG() {}
    RULoaderJPEG(const RULoaderJPEG&) = delete;
    RULoaderJPEG(RULoaderJPEG&&) = delete;
    RULoaderJPEG& operator=(const RULoaderJPEG&)= delete;
    RULoaderJPEG& operator=(RULoaderJPEG&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }

};


}

}

#endif
