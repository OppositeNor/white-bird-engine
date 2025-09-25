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
#ifndef __WBE_RU_LOADER_TEXT_HH__
#define __WBE_RU_LOADER_TEXT_HH__

#include "core/allocator/allocator.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"
#include "utils/utils.hh"
#include <string>
namespace WhiteBirdEngine {

/**
 * @class RULoaderText
 * @brief Resource unit loader for text resources.
 *
 */
class RULoaderText : public ResourceUnitLoader<RULoaderText> {
public:
    RULoaderText() = default;
    ~RULoaderText() {}
    RULoaderText(const RULoaderText&) = delete;
    RULoaderText(RULoaderText&&) = delete;
    RULoaderText& operator=(const RULoaderText&)= delete;
    RULoaderText& operator=(RULoaderText&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserData<ParserDataType>& p_parser_data) {
        // TODO: Test
        std::string key = p_parser_data.template get_value<std::string>("key");
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));
        RUText ru_text{};
        ru_text.encode_type = get_encode_type_from_str(p_parser_data.template get_value<std::string>("encode_type"));
        std::string str = load_text_file(static_cast<std::string>(path).c_str(), ru_text.encode_type);
        ru_text.size = str.size();
        MemID data_id = p_chunk.push_data(ru_text.size, (void*)str.c_str());
        ru_text.data = data_id;
        return p_chunk.push_res_unit(key, ru_text);
    }
};
}

#endif
