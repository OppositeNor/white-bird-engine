#ifndef __WBE_RU_LOADER_PPM_H__
#define __WBE_RU_LOADER_PPM_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
#include "resource/resource_unit_loader/ru_loader_ppm3.h"
namespace WhiteBirdEngine {

/**
 * @class RULoaderHDR
 * @brief Resource unit loader for PPM images.
 *
 */
class RULoaderPPM : public ResourceUnitLoader {
public:
    RULoaderPPM() = default;
    virtual ~RULoaderPPM() override {}
    RULoaderPPM(const RULoaderPPM&) = delete;
    RULoaderPPM(RULoaderPPM&&) = delete;
    RULoaderPPM& operator=(const RULoaderPPM&)= delete;
    RULoaderPPM& operator=(RULoaderPPM&&) = delete;

    virtual void load(ResourceChunk& p_chunk, const std::string& p_res_key, const Path& p_path) override;

private:
    RULoaderPPM3 ppm3_loader;
};

}

#endif
