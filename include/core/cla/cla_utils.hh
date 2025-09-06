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
#ifndef __WBE_CLA_TOKEN_HH__
#define __WBE_CLA_TOKEN_HH__

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

/**
 * @class CLARoot
 * @brief The root informations of the CLA.
 *
 */
struct CLARoot {
    /**
     * @brief The name of the utility.
     */
    std::string util_name;
    /**
     * @brief The operands.
     */
    std::vector<std::string> operands;
    /**
     * @brief The operations.
     */
    std::vector<struct CLAOperation> operations;
};

/**
 * @class CLAOperation
 * @brief The operation informations of the CLA.
 *
 */
struct CLAOperation {
    /**
     * @brief The name of the operation.
     */
    std::string operation_name;
    /**
     * @brief The arguments of the operation.
     */
    std::vector<std::string> arguments;
};

}

#endif
