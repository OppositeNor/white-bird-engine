/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_CLA_LEXER_HH__
#define __WBE_CLA_LEXER_HH__

#include "utils/defs.hh"
#include "cla_utils.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class CLALexer
 * @brief Lexer for CLA.
 *
 */
class CLALexer {
public:
    CLALexer() = default;
    ~CLALexer() {}
    CLALexer(const CLALexer&) = default;
    CLALexer(CLALexer&&) = default;
    CLALexer& operator=(const CLALexer&) = default;
    CLALexer& operator=(CLALexer&&) = default;

    /**
     * @brief Apply lexing.
     *
     * @todo Test
     * @param p_argc The number of the CLAs.
     * @param p_argv The argument list.
     */
    std::vector<CLAToken> apply(uint32_t p_argc, const char* p_argv[]) {
        WBE_DEBUG_ASSERT(p_argc >= 1);
        std::vector<CLAToken> result;
        result.reserve(p_argc);
        result.push_back({ .value = std::string(p_argv[0]), .type = CLAToken::Type::UTILITY_NAME });
        for (uint32_t i = 1; i < p_argc; ++i) {
            CLAToken token{};
            token.value = std::string(p_argv[i]);
            token.type = get_token_type(token.value);
            result.push_back(std::move(token));
        }
        return result;
    }

private:

    CLAToken::Type get_token_type(const std::string& p_token_value) {
        if (p_token_value[0] == '-') {
            if (p_token_value.size() > 1 && p_token_value[1] == '-') {
                return CLAToken::Type::OPTION_LONG;
            }
            return CLAToken::Type::OPTION_SHORT;
        }
        return CLAToken::Type::OPERAND;
    }
};

}

#endif
