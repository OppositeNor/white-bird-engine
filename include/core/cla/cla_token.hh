// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_CLA_TOKEN_HH__
#define __WBE_CLA_TOKEN_HH__

#include <string>
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
    enum class Type {
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


}

#endif
