#ifndef __WBE_RU_LOADER_JPEG_H__
#define __WBE_RU_LOADER_JPEG_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderJPEG
 * @brief Resource unit loader for JPEG images.
 *
 */
class RULoaderJPEG : public ResourceUnitLoader {
public:
    RULoaderJPEG() = default;
    virtual ~RULoaderJPEG() override {}
    RULoaderJPEG(const RULoaderJPEG&) = delete;
    RULoaderJPEG(RULoaderJPEG&&) = delete;
    RULoaderJPEG& operator=(const RULoaderJPEG&)= delete;
    RULoaderJPEG& operator=(RULoaderJPEG&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;

};


}

#endif
