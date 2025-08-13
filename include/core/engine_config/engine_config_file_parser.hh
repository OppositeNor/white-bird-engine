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
    config_options->single_tick_stack_size = parser.template get_value<size_t>("single_tick_stack_size");
    config_options->global_mem_pool_size = parser.template get_value<size_t>("global_mem_pool_size");
    config_options->thread_mem_pool_size = parser.template get_value<size_t>("thread_mem_pool_size");
}

}

#endif
