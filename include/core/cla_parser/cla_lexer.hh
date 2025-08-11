#ifndef __WBE_CLA_LEXER_HH__
#define __WBE_CLA_LEXER_HH__

#include <cstdint>
#include <string>
#include <vector>
namespace WhiteBirdEngine {

/**
 * @class CLAToken
 * @brief Token for parsing command line arguments.
 *
 */
struct CLAToken {
    // The value of the token.
    std::string value;

    /**
     * @brief Token type.
     */
    enum Type {
        // Utility name
        UTILITY_NAME,
        // Short option
        OPTION_SHORT,
        // Long option
        OPTION_LONG,
        // Operand
        OPERAND
    } type;
};

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
     * @param p_argc The number of the CLAs.
     * @param p_argv The argument list.
     */
    std::vector<std::string> apply(uint32_t p_argc, const char* p_argv[]) {
        std::vector<std::string> result;
        result.reserve(p_argc);
        for (uint32_t i = 0; i < p_argc; ++i) {
            result.push_back(std::string(p_argv[i]));
        }
        return result;
    }

private:

};

}

#endif
