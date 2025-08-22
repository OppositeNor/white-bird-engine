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
#ifndef __WBE_COMMAND_HH__
#define __WBE_COMMAND_HH__

#include <cstdint>
#include <string>
namespace WhiteBirdEngine {

using CommandID = uint64_t;

/**
 * @class Command
 * @brief Command class.
 *
 */
class Command {
public:
    Command() = default;
    virtual ~Command() {}

    /**
     * @brief Invoke an operation.
     */
    virtual void invoke() = 0;

    /**
     * @brief Devoke an operation.
     */
    virtual void devoke() = 0;

    /**
     * @brief Serialize the command.
     */
    virtual std::string& serialize() = 0;

    /**
     * @brief Deserialize the command.
     *
     * @param p_str The string of the command.
     */
    virtual void deserialize(const std::string& p_str) = 0;
};

}

#endif
