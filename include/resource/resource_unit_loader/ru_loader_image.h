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
class RULoaderImage : public ResourceUnitLoader<RULoaderImage> {
public:
    RULoaderImage() = default;
    ~RULoaderImage() {}
    RULoaderImage(const RULoaderImage&) = delete;
    RULoaderImage(RULoaderImage&&) = delete;
    RULoaderImage& operator=(const RULoaderImage&)= delete;
    RULoaderImage& operator=(RULoaderImage&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResChunkType& p_chunk, const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }

private:
    RULoaderPNG loader_png;
    RULoaderPPM loader_ppm;
    RULoaderHDR loader_hdr;
};

}

#endif
