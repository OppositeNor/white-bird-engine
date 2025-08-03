#ifndef __WBE_CLA_PARSER_H__
#define __WBE_CLA_PARSER_H__

#include "utils/interface/singleton.h"
#include "utils/utils.h"

namespace WhiteBirdEngine {

class EngineConfig;

/**
 * @class CLAParser
 * @brief Interface of parsers for command line arguments.
 * @todo Test
 */
class CLAParser : public Singleton<CLAParser> {
public:
    CLAParser(EngineConfigOptions& p_engine_config)
        : engine_config(&p_engine_config) {}
    virtual ~CLAParser() {}

    /**
     * @brief Parse the command line arguments.
     *
     * @param p_argc argc.
     * @param p_argv argv.
     */
    virtual void parse(int p_argc, char* p_argv[]);
private:
    EngineConfigOptions* engine_config;
};

}

#endif
