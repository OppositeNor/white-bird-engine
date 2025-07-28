#ifndef __WBE_RU_LOADER_PPM3_H__
#define __WBE_RU_LOADER_PPM3_H__

#include "resource/resource_unit_loader/resource_unit_loader.h"
#include <memory>
namespace WhiteBirdEngine {

/**
 * @class RULoaderPPM3
 * @brief Resource unit loader for ppm3 images.
 *
 */
class RULoaderPPM3 : public ResourceUnitLoader<RULoaderPPM3> {
public:
    RULoaderPPM3() = default;
    ~RULoaderPPM3() {}
    RULoaderPPM3(const RULoaderPPM3&) = delete;
    RULoaderPPM3(RULoaderPPM3&&) = delete;
    RULoaderPPM3& operator=(const RULoaderPPM3&)= delete;
    RULoaderPPM3& operator=(RULoaderPPM3&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }
private:
    std::unique_ptr<unsigned char> load_ppm3_content(const Path& p_path, bool p_flip_h, bool p_flip_v,
                                                     uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels);

};

}

#endif
