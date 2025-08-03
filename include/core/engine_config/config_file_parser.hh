#ifndef __WBE_CONFIG_FILE_PARSER_HH__
#define __WBE_CONFIG_FILE_PARSER_HH__

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
