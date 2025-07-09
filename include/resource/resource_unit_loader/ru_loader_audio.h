#ifndef __WBE_RU_LOADER_AUDIO_H__
#define __WBE_RU_LOADER_AUDIO_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderAudio
 * @brief Loader for audio resource units.
 *
 */
class RULoaderAudio : public ResourceUnitLoader<RULoaderAudio> {
public:
    RULoaderAudio() = default;
    ~RULoaderAudio() {}
    RULoaderAudio(const RULoaderAudio&) = delete;
    RULoaderAudio(RULoaderAudio&&) = delete;
    RULoaderAudio& operator=(const RULoaderAudio&)= delete;
    RULoaderAudio& operator=(RULoaderAudio&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResChunkType& p_chunk, const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }

};

}

#endif
