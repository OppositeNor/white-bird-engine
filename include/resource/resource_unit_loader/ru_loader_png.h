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
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        uint32_t desired_channels = 0;
        if (p_parser_data.contains("desired_channels")) {
            desired_channels = p_parser_data.template get_value<uint32_t>("desired_channels");
        }
        RUImage ru_image{};
        if (desired_channels != 0) {
            ru_image.channel_count = desired_channels;
        }
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        ru_image.config.set(RUImage::IS_SRGB, properties.contains("is_srgb") && properties.template get_value<bool>("is_srgb"));
        ru_image.config.set(RUImage::IS_HDR, false);
        bool flip_h = properties.contains("flip_h") && properties.template get_value<bool>("flip_h");
        bool flip_v = properties.contains("flip_v") && properties.template get_value<bool>("flip_v");
        auto data = load_image_data(get_resource_path(p_parser_data.template get_value<std::string>("path")),
                        desired_channels, flip_h, flip_v, ru_image.width, ru_image.height, ru_image.channel_count);
        ru_image.image_data = p_chunk.push_data(ru_image.width * ru_image.height * ru_image.channel_count, data.get(), p_allocator);
        return p_chunk.template push_res_unit<RUImage>(p_parser_data.template get_value<std::string>("key"), ru_image);
    }

private:
    std::unique_ptr<unsigned char> load_image_data(const Path& p_path, uint32_t p_desired_channels, bool p_flip_h, bool p_flip_v,
                                                   uint32_t& p_width, uint32_t& p_height, uint32_t& p_file_channels);
};


}

#endif
