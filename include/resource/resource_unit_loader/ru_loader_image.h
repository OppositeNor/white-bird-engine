#ifndef __WBE_RU_LOADER_IMAGE_H__
#define __WBE_RU_LOADER_IMAGE_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
#include "resource/resource_unit_loader/ru_loader_hdr.h"
#include "resource/resource_unit_loader/ru_loader_png.h"
#include "resource/resource_unit_loader/ru_loader_ppm.h"

namespace WhiteBirdEngine {

/**
 * @class RULoaderHDR
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

    virtual void load(ResourceChunk& p_chunk, const std::string& p_res_key, const Path& p_path) override;
private:
    RULoaderPNG loader_png;
    RULoaderPPM loader_ppm;
    RULoaderHDR loader_hdr;
};

}

#endif
