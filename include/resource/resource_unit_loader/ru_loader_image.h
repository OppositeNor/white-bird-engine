#ifndef __WBE_RU_LOADER_IMAGE_H__
#define __WBE_RU_LOADER_IMAGE_H__

#include "core/allocator/allocator.h"
#include "resource/resource_unit_loader/resource_unit_loader.h"
#include "resource/resource_unit_loader/ru_loader_hdr.h"
#include "resource/resource_unit_loader/ru_loader_png.h"
#include "resource/resource_unit_loader/ru_loader_ppm.h"

namespace WhiteBirdEngine {

/**
 * @class RULoaderImage
 * @brief Resource unit loader for images.
 *
 */
class RULoaderImage : public ResourceUnitLoader {
public:
    RULoaderImage() = default;
    virtual ~RULoaderImage() override {}
    RULoaderImage(const RULoaderImage&) = delete;
    RULoaderImage(RULoaderImage&&) = delete;
    RULoaderImage& operator=(const RULoaderImage&)= delete;
    RULoaderImage& operator=(RULoaderImage&&) = delete;

    virtual MemID load(ResourceChunk& p_chunk, const Path& p_path) override;
private:
    RULoaderPNG loader_png;
    RULoaderPPM loader_ppm;
    RULoaderHDR loader_hdr;
};

}

#endif
