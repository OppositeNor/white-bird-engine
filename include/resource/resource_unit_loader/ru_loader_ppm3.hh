#ifndef __WBE_RU_LOADER_PPM3_HH__
#define __WBE_RU_LOADER_PPM3_HH__

#include "resource/resource_unit/ru_image.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
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
        // TODO: Test
        std::string key = p_parser_data.template get_value<std::string>("key");
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));

        RUImage ru_image{};
        ru_image.color_space = get_color_space(properties.template get_value<std::string>("color_space"));
        ru_image.config.set(RUImage::IS_HDR, false);
        bool flip_h = properties.contains("flip_h") && properties.template get_value<bool>("flip_h");
        bool flip_v = properties.contains("flip_v") && properties.template get_value<bool>("flip_v");
        auto data = load_ppm3_content(path, flip_h, flip_v, ru_image.width, ru_image.height);
        ru_image.channel_count = 3;
        ru_image.image_data = p_chunk.push_data(ru_image.width * ru_image.height * ru_image.channel_count, data.get(), p_allocator);
        return p_chunk.template push_res_unit<RUImage>(key, ru_image);
    }
private:
    std::unique_ptr<unsigned char[]> load_ppm3_content(const Path& p_path, bool p_flip_h, bool p_flip_v,
                                                     uint32_t& p_width, uint32_t& p_height);

};

}

#endif
