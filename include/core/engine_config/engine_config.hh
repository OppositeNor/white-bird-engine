#ifndef __WBE_ENGINE_CONFIG_HH__
#define __WBE_ENGINE_CONFIG_HH__

#include "core/cla/cla_parser.hh"
#include "core/engine_config/engine_config_file_parser.hh"
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
     * @param p_config_file_path The path to the config file.
     * @param p_argc argc
     * @param p_argv argv
     */
    EngineConfig(const Path& p_config_file_path, int p_argc, char* p_argv[])
        : config_file_parser(config_options, ParserYAML()) {
        parse_config_file(p_config_file_path);
        parse_cla(p_argc, p_argv);
    }
    virtual ~EngineConfig() {}
    EngineConfig(const EngineConfig&) = delete;
    EngineConfig(EngineConfig&&) = delete;
    EngineConfig& operator=(const EngineConfig&) = delete;
    EngineConfig& operator=(EngineConfig&&) = delete;

    /**
     * @brief Get the configuration options.
     *
     * @return The configuration options.
     */
    const EngineConfigOptions& get_config_options(){
        return config_options;
    }

private:
    EngineConfigFileParser<ParserYAML> config_file_parser;
    EngineConfigOptions config_options;

    void parse_cla(int p_argc, char* p_argv[]) {
        // TODO
    }

    void parse_config_file(const Path& p_path) {
        config_file_parser.parse(p_path);
    }

};
}

#endif
