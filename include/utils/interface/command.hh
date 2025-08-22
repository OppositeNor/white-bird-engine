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
