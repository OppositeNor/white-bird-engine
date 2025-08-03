#ifndef __WBE_RU_LOADER_JPEG_HH__
#define __WBE_RU_LOADER_JPEG_HH__

#include "resource/resource_unit/ru_image.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_utils.hh"
namespace WhiteBirdEngine {

/**
 * @class RULoaderJPEG
 * @brief Resource unit loader for jpeg images.
 *
 */
class RULoaderJPEG : public ResourceUnitLoader<RULoaderJPEG> {
public:
    RULoaderJPEG() = default;
    ~RULoaderJPEG() {}
    RULoaderJPEG(const RULoaderJPEG&) = delete;
    RULoaderJPEG(RULoaderJPEG&&) = delete;
    RULoaderJPEG& operator=(const RULoaderJPEG&)= delete;
    RULoaderJPEG& operator=(RULoaderJPEG&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        std::string key = p_parser_data.template get_value<std::string>("key");
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));

        RUImage ru_image{};
        ru_image.color_space = get_color_space(properties.template get_value<std::string>("color_space"));
        ru_image.config.set(RUImage::IS_HDR, false);
        uint32_t desired_channels = 0;
        if (p_parser_data.contains("desired_channels")) {
            desired_channels = properties.template get_value<uint32_t>("desired_channels");
        }
        bool flip_h = properties.contains("flip_h") && properties.template get_value<bool>("flip_h");
        bool flip_v = properties.contains("flip_v") && properties.template get_value<bool>("flip_v");
        auto data = load_image_data(path, desired_channels, flip_h, flip_v, ru_image.width, ru_image.height, ru_image.channel_count);
        if (desired_channels != 0) {
            ru_image.channel_count = desired_channels;
        }
        ru_image.image_data = p_chunk.push_data(ru_image.width * ru_image.height * ru_image.channel_count, data.get(), p_allocator);
        return p_chunk.template push_res_unit<RUImage>(key, ru_image);
    }

};


}

#endif
