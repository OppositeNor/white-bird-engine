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
#ifndef __WBE_COMMAND_BUFFER_HH__
#define __WBE_COMMAND_BUFFER_HH__

#include "core/command/command.hh"
#include "core/memory/reference_strong.hh"
namespace WhiteBirdEngine {

/**
 * @class CommandBuffer
 * @brief Stores a buffer that contians all the command for an instance to execute.
 * @note This has nothing to do with the command buffer in rendering APIs.
 */
class CommandBuffer {
public:
    CommandBuffer() = default;
    virtual ~CommandBuffer() {}

    /**
     * @brief Retrieve a command from a buffer.
     */
    virtual Ref<Command> retrieve_command() = 0;

    /**
     * @brief Add a command to the buffer.
     *
     * @param p_command The command to add to the buffer.
     */
    virtual void add_command(Ref<Command> p_command) = 0;
};

}

#endif
