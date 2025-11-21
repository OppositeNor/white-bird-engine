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
#ifndef __WBE_RU_LOADER_AUDIO_HH__
#define __WBE_RU_LOADER_AUDIO_HH__

#include "resource/resource_unit_loader/resource_unit_loader.hh"
namespace WhiteBirdEngine {

/**
 * @class RULoaderAudio
 * @brief Loader for audio resource units.
 *
 */
class RULoaderAudio : public ResourceUnitLoader<RULoaderAudio> {
public:
    RULoaderAudio() = default;
    ~RULoaderAudio() {}
    RULoaderAudio(const RULoaderAudio&) = delete;
    RULoaderAudio(RULoaderAudio&&) = delete;
    RULoaderAudio& operator=(const RULoaderAudio&)= delete;
    RULoaderAudio& operator=(RULoaderAudio&&) = delete;

    template <ParserDataConcept ParserDataType>
    RefRes<char> load(ResourceChunk& p_chunk, const std::string& p_key, const ParserDataType& p_parser_data) {
        return MEM_NULL; // TODO
    }

};

}

#endif
