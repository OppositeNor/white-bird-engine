#ifndef __WBE_RU_LOADER_AUDIO_H__
#define __WBE_RU_LOADER_AUDIO_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderAudio
 * @brief Loader for audio resource units.
 *
 */
class RULoaderAudio : public ResourceUnitLoader {
public:
    RULoaderAudio() = default;
    virtual ~RULoaderAudio() override {}
    RULoaderAudio(const RULoaderAudio&) = delete;
    RULoaderAudio(RULoaderAudio&&) = delete;
    RULoaderAudio& operator=(const RULoaderAudio&)= delete;
    RULoaderAudio& operator=(RULoaderAudio&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;

};

}

#endif
