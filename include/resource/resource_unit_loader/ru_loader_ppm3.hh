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
#ifndef __WBE_RU_LOADER_PPM3_HH__
#define __WBE_RU_LOADER_PPM3_HH__

#include "resource/resource_unit/resource_unit.hh"
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

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const std::string& p_key, const ParserData<ParserDataType>& p_parser_data) {
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));
        RUImage ru_image{};
        ru_image.color_space = get_color_space(p_parser_data.template get_value<std::string>("color_space"));
        ru_image.config.set(RUImage::IS_HDR, false);
        bool flip_h = p_parser_data.contains("flip_h") && p_parser_data.template get_value<bool>("flip_h");
        bool flip_v = p_parser_data.contains("flip_v") && p_parser_data.template get_value<bool>("flip_v");
        auto data = load_ppm3_content(path, flip_h, flip_v, ru_image.width, ru_image.height);
        ru_image.channel_count = 3;
        ru_image.image_data = p_chunk.push_data(ru_image.width * ru_image.height * ru_image.channel_count, data.get());
        return p_chunk.template push_res_unit<RUImage>(p_key, ru_image);
    }
private:
    std::unique_ptr<unsigned char[]> load_ppm3_content(const Path& p_path, bool p_flip_h, bool p_flip_v,
                                                     uint32_t& p_width, uint32_t& p_height);

};

}

#endif
