#ifndef __WBE_COMMAND_HH__
#define __WBE_COMMAND_HH__

namespace WhiteBirdEngine {

struct Command {
    Command();
    virtual ~Command() {}

    /**
     * @brief Perform the command.
     */
    virtual void perform() = 0;
};

}

#endif
