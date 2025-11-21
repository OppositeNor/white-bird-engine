/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_RU_LOADER_JPEG_HH__
#define __WBE_RU_LOADER_JPEG_HH__

#include "resource/resource_unit/resource_unit.hh"
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

    template <ParserDataConcept ParserDataType>
    RefRes<char> load(ResourceChunk& p_chunk, const std::string& p_key, const ParserDataType& p_parser_data) {
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));

        RUImage ru_image{};
        ru_image.color_space = get_color_space(p_parser_data.template get_value<std::string>("color_space"));
        ru_image.config.set(RUImage::IS_HDR, false);
        uint32_t desired_channels = 0;
        if (p_parser_data.contains("desired_channels")) {
            desired_channels = p_parser_data.template get_value<uint32_t>("desired_channels");
        }
        bool flip_h = p_parser_data.contains("flip_h") && p_parser_data.template get_value<bool>("flip_h");
        bool flip_v = p_parser_data.contains("flip_v") && p_parser_data.template get_value<bool>("flip_v");
        auto data = load_image_data(path, desired_channels, flip_h, flip_v, ru_image.width, ru_image.height, ru_image.channel_count);
        if (desired_channels != 0) {
            ru_image.channel_count = desired_channels;
        }
        ru_image.image_data = p_chunk.push_data(ru_image.width * ru_image.height * ru_image.channel_count, data.get());
        return p_chunk.template push_res_unit<RUImage>(p_key, ru_image);
    }

};


}

#endif
