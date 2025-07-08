#ifndef __WBE_RU_LOADER_TEXT_H__
#define __WBE_RU_LOADER_TEXT_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderText
 * @brief Resource unit loader for text resources.
 *
 */
class RULoaderText : public ResourceUnitLoader {
public:
    RULoaderText() = default;
    virtual ~RULoaderText() override {}
    RULoaderText(const RULoaderText&) = delete;
    RULoaderText(RULoaderText&&) = delete;
    RULoaderText& operator=(const RULoaderText&)= delete;
    RULoaderText& operator=(RULoaderText&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;

};


}

#endif
