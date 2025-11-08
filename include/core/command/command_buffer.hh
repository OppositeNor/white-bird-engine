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
