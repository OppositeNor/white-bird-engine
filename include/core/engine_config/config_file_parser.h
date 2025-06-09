#ifndef __WBE_CONFIG_FILE_PARSER_H__
#define __WBE_CONFIG_FILE_PARSER_H__

#include "platform/file_system/path.h"
#include "utils/utils.h"

namespace WhiteBirdEngine {

/**
 * @class ConfigFileParser
 * @brief Interface of parsers for config file.
 */
class ConfigFileParser {
public:
    ConfigFileParser() = default;
    virtual ~ConfigFileParser() {}

    /**
     * @brief Set the engine config options.
     *
     * @param p_config_options The engine config options.
     */
    virtual void set_engine_config_options(EngineConfigOptions* p_config_options) = 0;

    /**
     * @brief Parse a config file from a path.
     *
     * @param p_path The path of the config file.
     */
    virtual void parse(const Path& p_path) = 0;
};

}

#endif
