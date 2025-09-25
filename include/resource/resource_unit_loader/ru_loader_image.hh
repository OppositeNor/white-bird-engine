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
#ifndef __WBE_RU_LOADER_IMAGE_HH__
#define __WBE_RU_LOADER_IMAGE_HH__

#include "core/allocator/allocator.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_unit_loader/ru_loader_hdr.hh"
#include "resource/resource_unit_loader/ru_loader_jpeg.hh"
#include "resource/resource_unit_loader/ru_loader_png.hh"
#include "resource/resource_unit_loader/ru_loader_ppm.hh"

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

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserData<ParserDataType>& p_parser_data) {
        std::string ext = FileSystem::get_ext(get_resource_path(p_parser_data.template get_value<std::string>("path")));
        if (ext == "png") {
            return RULoaderPNG().load(p_chunk, p_parser_data);
        }
        if (ext == "jpg" || ext == "jpeg") {
            return RULoaderJPEG().load(p_chunk, p_parser_data);
        }
        if (ext == "ppm") {
            return RULoaderPPM().load(p_chunk, p_parser_data);
        }
        if (ext == "hdr") {
            return RULoaderHDR().load(p_chunk, p_parser_data);
        }
        throw std::runtime_error("Failed to load image. Image with extension: " + ext + " not supported.");
    }

};

}

#endif
