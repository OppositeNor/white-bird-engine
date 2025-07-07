#ifndef __WBE_RU_LOADER_PNG_H__
#define __WBE_RU_LOADER_PNG_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderPNG
 * @brief Resource unit loader for PNG images.
 *
 */
class RULoaderPNG : public ResourceUnitLoader {
public:
    RULoaderPNG() = default;
    virtual ~RULoaderPNG() override {}
    RULoaderPNG(const RULoaderPNG&) = delete;
    RULoaderPNG(RULoaderPNG&&) = delete;
    RULoaderPNG& operator=(const RULoaderPNG&)= delete;
    RULoaderPNG& operator=(RULoaderPNG&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;

};

}

#endif
