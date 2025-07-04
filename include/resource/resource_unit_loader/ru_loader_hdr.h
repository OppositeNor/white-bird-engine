#ifndef __WBE_RU_LOADER_HDR_H__
#define __WBE_RU_LOADER_HDR_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderHDR
 * @brief Resource unit loader for HDR images.
 *
 */
class RULoaderHDR : public ResourceUnitLoader {
public:
    RULoaderHDR() = default;
    virtual ~RULoaderHDR() override {}
    RULoaderHDR(const RULoaderHDR&) = delete;
    RULoaderHDR(RULoaderHDR&&) = delete;
    RULoaderHDR& operator=(const RULoaderHDR&)= delete;
    RULoaderHDR& operator=(RULoaderHDR&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;

};

}

#endif
