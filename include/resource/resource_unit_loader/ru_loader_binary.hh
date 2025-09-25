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
#ifndef __WBE_RU_LOADER_BINARY_HH__
#define __WBE_RU_LOADER_BINARY_HH__

#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"

namespace WhiteBirdEngine {

class RULoaderBinary : public ResourceUnitLoader<RULoaderBinary> {
public:
    RULoaderBinary() = default;
    ~RULoaderBinary() {}
    RULoaderBinary(const RULoaderBinary&) = delete;
    RULoaderBinary(RULoaderBinary&&) = delete;
    RULoaderBinary& operator=(const RULoaderBinary&)= delete;
    RULoaderBinary& operator=(RULoaderBinary&&) = delete;

    template <ParserDataConcept ParserDataType>
    MemID load(ResourceChunk& p_chunk, const ParserData<ParserDataType>& p_parser_data) {
        // TODO: Test
        std::string key = p_parser_data.template get_value<std::string>("key");
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));
        RUBinary ru_binary{};
        std::vector<char> str = load_binary_file(static_cast<std::string>(path).c_str());
        ru_binary.size = str.size();
        MemID data_id = p_chunk.push_data(ru_binary.size, str.data());
        ru_binary.data = data_id;
        return p_chunk.push_res_unit(key, ru_binary);
    }
};

}

#endif
