#ifndef __WBE_ENGINE_CONFIG_HH__
#define __WBE_ENGINE_CONFIG_HH__

#include "core/engine_config/cla_parser.hh"
#include "core/engine_config/config_file_parser.hh"
#include "core/parser/parser_yaml.hh"
#include "platform/file_system/path.hh"
#include "utils/interface/singleton.hh"
#include "utils/utils.hh"

namespace WhiteBirdEngine {

/**
 * @class EngineConfig
 * @brief Engine configuration class.
 *
 */
class EngineConfig : public Singleton<EngineConfig> {
public:

    /**
     * @brief Constructor.
     *
     * @param p_engine_config The configuration options.
     * @param p_config_file_parser The parser for the config file.
     */
    EngineConfig(EngineConfigOptions& p_engine_config)
        : cla_parser(p_engine_config), config_file_parser(p_engine_config, ParserYAML()) {
    }
    virtual ~EngineConfig() {}
    EngineConfig(const EngineConfig&) = delete;
    EngineConfig(EngineConfig&&) = delete;
    EngineConfig& operator=(const EngineConfig&) = delete;
    EngineConfig& operator=(EngineConfig&&) = delete;

    /**
     * @brief Parse the command line argument configurations.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     */
    void parse_cla(int p_argc, char* p_argv[]) {
        cla_parser.parse(p_argc, p_argv);
    }

    
    /**
     * @brief Parse the config file from a path.
     *
     * @param p_path The path to the config file.
     */
    void parse_config_file(const Path& p_path) {
        config_file_parser.parse(p_path);
    }

private:
    CLAParser cla_parser;
    ConfigFileParser<ParserYAML> config_file_parser;
};

}

#endif
