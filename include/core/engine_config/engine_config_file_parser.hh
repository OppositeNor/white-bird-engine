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
#ifndef __WBE_ENGINE_CONFIG_FILE_PARSER_HH__
#define __WBE_ENGINE_CONFIG_FILE_PARSER_HH__

#include "core/parser/parser.hh"
#include "platform/file_system/path.hh"
#include "utils/utils.hh"
#include "utils/interface/singleton.hh"

namespace WhiteBirdEngine {

/**
 * @class ConfigFileParser
 * @brief Interface of parsers for config file.
 */
template <ParserConcept ParserType>
class EngineConfigFileParser : public Singleton<EngineConfigFileParser<ParserType>> {
public:
    EngineConfigFileParser(EngineConfigOptions& p_config_options, ParserType p_parser)
        : config_options(&p_config_options), parser(p_parser) {
    }
    ~EngineConfigFileParser() {}

    /**
     * @brief Parse a config file from a path.
     *
     * @param p_path The path of the config file.
     */
    void parse(const Path& p_path);

private:
    EngineConfigOptions* config_options;
    ParserType parser;
};

template <ParserConcept ParserType>
void EngineConfigFileParser<ParserType>::parse(const Path& p_path) {
    parser.parse(p_path);
}

}

#endif
