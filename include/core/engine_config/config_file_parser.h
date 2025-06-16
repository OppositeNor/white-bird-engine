#ifndef __WBE_CONFIG_FILE_PARSER_H__
#define __WBE_CONFIG_FILE_PARSER_H__

#include "core/parser/parser.h"
#include "platform/file_system/path.h"
#include "utils/utils.h"
#include "utils/interface/singleton.h"

namespace WhiteBirdEngine {

/**
 * @class ConfigFileParser
 * @brief Interface of parsers for config file.
 */
template <ParserConcept ParserType>
class ConfigFileParser : public Singleton<ConfigFileParser<ParserType>> {
public:
    ConfigFileParser(EngineConfigOptions& p_config_options, ParserType p_parser)
        : config_options(&p_config_options), parser(std::move(p_parser)) {
    }
    virtual ~ConfigFileParser() {}

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
void ConfigFileParser<ParserType>::parse(const Path& p_path) {
    // TODO
}

}

#endif
