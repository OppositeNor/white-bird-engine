#ifndef __WBE_CLA_PARSER_HH__
#define __WBE_CLA_PARSER_HH__

#include "core/cla/cla_token.hh"
#include "utils/interface/singleton.hh"
#include <vector>

namespace WhiteBirdEngine {

class EngineConfig;

class CLAAST {
    // TODO
};

/**
 * @class CLAParser
 * @brief Interface of parsers for command line arguments.
 * @todo Test
 */
class CLAParser : public Singleton<CLAParser> {
public:
    CLAParser() = default;
    ~CLAParser() {}
    CLAParser(const CLAParser&) = default;
    CLAParser(CLAParser&&) = default;
    CLAParser& operator=(const CLAParser&) = default;
    CLAParser& operator=(CLAParser&&) = default;

    CLAAST parse(const std::vector<CLAToken>& p_tokens);
    
private:
};

}

#endif
