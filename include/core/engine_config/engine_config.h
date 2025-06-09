#ifndef __WBE_ENGINE_CONFIG_H__
#define __WBE_ENGINE_CONFIG_H__

#include "core/engine_config/cla_parser.h"
#include "core/engine_config/config_file_parser.h"
#include "platform/file_system/path.h"
#include "utils/defs.h"
#include "utils/utils.h"
#include <memory>

namespace WhiteBirdEngine {

class EngineConfig {
public:

    EngineConfig(std::unique_ptr<CLAParser> p_cla_parser, std::unique_ptr<ConfigFileParser> p_config_file_parser)
        : cla_parser(std::move(p_cla_parser)), config_file_parser(std::move(p_config_file_parser)) {
        WBE_DEBUG_ASSERT(cla_parser != nullptr);
        WBE_DEBUG_ASSERT(config_file_parser != nullptr);
        cla_parser->set_engine_config_options(&options);
        config_file_parser->set_engine_config_options(&options);
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
    void parse_cla(char* p_argc[], int p_argv) {
        cla_parser->parse(p_argc, p_argv);
    }

    
    /**
     * @brief Parse the config file from a path.
     *
     * @param p_path The path to the config file.
     */
    void parse_config_file(const Path& p_path) {
        config_file_parser->parse(p_path);
    }

private:
    // TODO: configuration properties.
    EngineConfigOptions options;
    std::unique_ptr<CLAParser> cla_parser;
    std::unique_ptr<ConfigFileParser> config_file_parser;

};

}

#endif
