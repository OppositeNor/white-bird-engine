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
#ifndef __WBE_RU_LOADER_PPM_HH__
#define __WBE_RU_LOADER_PPM_HH__

#include "platform/file_system/path.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "resource/resource_unit_loader/ru_loader_ppm3.hh"
#include <stdexcept>
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RULoaderPPM
 * @brief Resource unit loader for ppm images.
 *
 */
class RULoaderPPM : public ResourceUnitLoader<RULoaderPPM> {
public:
    RULoaderPPM() = default;
    ~RULoaderPPM() {}
    RULoaderPPM(const RULoaderPPM&) = delete;
    RULoaderPPM(RULoaderPPM&&) = delete;
    RULoaderPPM& operator=(const RULoaderPPM&)= delete;
    RULoaderPPM& operator=(RULoaderPPM&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserData<ParserDataType>& p_parser_data) {
        // TODO: Test
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));
        auto header = get_ppm_header(path);
        if (header == "P3") {
            return loader_ppm3.load(p_chunk, p_parser_data);
        }
        throw std::runtime_error("Failed to load PPM: header: " + header + " not supported by White Bird Enigne.");
    }

private:
    RULoaderPPM3 loader_ppm3;
    std::string get_ppm_header(const Path& p_path);
};


}

#endif
