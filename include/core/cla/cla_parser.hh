// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_CLA_PARSER_HH__
#define __WBE_CLA_PARSER_HH__

#include "core/cla/cla_ast.hh"
#include "core/cla/cla_token.hh"
#include "core/memory/reference_strong.hh"
#include "utils/interface/singleton.hh"
#include <vector>

namespace WhiteBirdEngine {

class EngineConfig;

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

    /**
     * @brief Parse tokens into ast.
     *
     * @param p_tokens The tokens to parse.
     * @return 
     */
    Ref<CLAASTNode> parse(const std::vector<CLAToken>& p_tokens);
    
private:
};

}

#endif
