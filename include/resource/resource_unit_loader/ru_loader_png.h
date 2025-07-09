#ifndef __WBE_RU_LOADER_PNG_H__
#define __WBE_RU_LOADER_PNG_H__

#include "platform/file_system/path.h"
#include "resource/resource_unit_loader/resource_unit_loader.h"
#include <cstdint>
#include <memory>

namespace WhiteBirdEngine {

/**
 * @class RULoaderPNG
 * @brief Resource unit loader for png images.
 *
 */
class RULoaderPNG : public ResourceUnitLoader<RULoaderPNG> {
public:
    RULoaderPNG() = default;
    ~RULoaderPNG() {}
    RULoaderPNG(const RULoaderPNG&) = delete;
    RULoaderPNG(RULoaderPNG&&) = delete;
    RULoaderPNG& operator=(const RULoaderPNG&)= delete;
    RULoaderPNG& operator=(RULoaderPNG&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserDataType& p_parser_data) {
        return MEM_NULL; // TODO
    }

private:
    std::unique_ptr<unsigned char> load_image_data(const Path& p_path, uint32_t p_desired_channels,
                                                   uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels);
};


}

#endif
