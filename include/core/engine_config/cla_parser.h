#ifndef __WBE_CLA_PARSER_H__
#define __WBE_CLA_PARSER_H__

#include "utils/utils.h"
namespace WhiteBirdEngine {

class EngineConfig;

/**
 * @class CLAParser
 * @brief Interface of parsers for command line arguments.
 *
 */
class CLAParser {
public:
    CLAParser() = default;
    virtual ~CLAParser() {}

    /**
     * @brief Set the engine config options.
     *
     * @param p_config_options The engine config options.
     */
    virtual void set_engine_config_options(EngineConfigOptions* p_config_options) = 0;

    /**
     * @brief Parse the command line arguments.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     */
    virtual void parse(int p_argc, char* p_argv[]) = 0;
};

}

#endif
