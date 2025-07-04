#ifndef __WBE_RU_LOADER_PPM3_H__
#define __WBE_RU_LOADER_PPM3_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderHDR
 * @brief Resource unit loader for PPM P3 images.
 *
 */
class RULoaderPPM3 : public ResourceUnitLoader {
public:
    RULoaderPPM3() = default;
    virtual ~RULoaderPPM3() override {}
    RULoaderPPM3(const RULoaderPPM3&) = delete;
    RULoaderPPM3(RULoaderPPM3&&) = delete;
    RULoaderPPM3& operator=(const RULoaderPPM3&)= delete;
    RULoaderPPM3& operator=(RULoaderPPM3&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;

};

}

#endif
