#ifndef __WBE_COMMAND_H__
#define __WBE_COMMAND_H__

#include <cstdint>
#include <string>
namespace WhiteBirdEngine {

using CommandID = uint64_t;

class Command {
public:
    Command() = default;
    virtual ~Command() {}

    virtual void invoke() = 0;
    virtual void devoke() = 0;

    virtual std::string serialize() = 0;
    virtual void deserialize(const std::string& p_str) = 0;
};

}

#endif
