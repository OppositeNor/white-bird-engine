#ifndef __WBE_RU_LOADER_PNG_H__
#define __WBE_RU_LOADER_PNG_H__

#include "platform/file_system/path.h"
#include "resource/resource_unit/ru_image.h"
#include "resource/resource_unit_loader/resource_unit_loader.h"
#include "resource/resource_utils.h"
#include <cstdint>
#include <memory>

namespace WhiteBirdEngine {

/**
 * @class RULoaderPNG
 * @brief Resource unit loader for png images.
 *
 * @todo Test
 */
class RULoaderPNG : public ResourceUnitLoader<RULoaderPNG> {
public:
    RULoaderPNG() = default;
    ~RULoaderPNG() {}
    RULoaderPNG(const RULoaderPNG&) = delete;
    RULoaderPNG(RULoaderPNG&&) = delete;
    RULoaderPNG& operator=(const RULoaderPNG&)= delete;
    RULoaderPNG& operator=(RULoaderPNG&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResChunkType& p_chunk, const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
        uint32_t desired_channels = 0;
        if (p_parser_data.contains("desired_channels")) {
            desired_channels = p_parser_data.template get<uint32_t>("desired_channels");
        }
        RUImage ru_image{};
        auto data = load_image_data(get_resource_path(p_parser_data.template get<std::string>("path")),
                        desired_channels, ru_image.width, ru_image.height, ru_image.channel_count);
        if (desired_channels != 0) {
            ru_image.channel_count = desired_channels;
        }
        auto properties = p_parser_data.template get<ParserDataType>("properties");
        ru_image.config.set(RUImage::IS_SRGB, properties.contains("is_srgb") && properties.template get<bool>("is_srgb"));
        ru_image.config.set(RUImage::IS_HDR, false);
        ru_image.image_data = p_chunk.push_data(ru_image.width * ru_image.height * ru_image.channel_count, data.get(), p_allocator);
        return p_chunk.template push_res_unit<RUImage>(p_parser_data.template get<std::string>("key"), ru_image);
    }

private:
    std::unique_ptr<unsigned char> load_image_data(const Path& p_path, uint32_t p_desired_channels,
                                                   uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels);
};


}

#endif
