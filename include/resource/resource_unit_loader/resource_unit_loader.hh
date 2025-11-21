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
#ifndef __WBE_RESOURCE_UNIT_LOADER_HH__
#define __WBE_RESOURCE_UNIT_LOADER_HH__

#include "core/parser/parser.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "utils/defs.hh"
#include <string>

namespace WhiteBirdEngine {

/**
 * @class ResourceUnitLoader
 * @brief Resource unit loader.
 *
 */
template <typename ChildT>
class ResourceUnitLoader {
public:
    ResourceUnitLoader() = default;
    ~ResourceUnitLoader() {}

    /**
     * @brief Load a resource to a chunk.
     *
     * @param p_chunk The chunk to load resource to.
     * @param p_parser_data The parser data.
     */
    template <ParserDataConcept ParserDataType>
    RefRes<char> load(ResourceChunk& p_chunk, const std::string& p_key, const ParserDataType& p_parser_data) {
        return static_cast<ChildT*>(this)->template load<ParserDataType>(p_chunk, p_parser_data);
    }
};

WBE_DECL_CRTP_CONCEPT(ResourceUnitLoader);

}


#endif
