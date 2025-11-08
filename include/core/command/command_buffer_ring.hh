#ifndef __WBE_COMMAND_BUFFER_RING_HH__
#define __WBE_COMMAND_BUFFER_RING_HH__

#include "command_buffer.hh"
#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class CommandBufferRing
 * @brief Command buffer, ring buffer version.
 *
 */
class CommandBufferRing final : public CommandBuffer {
public:
    CommandBufferRing(size_t p_buffer_size);
    virtual ~CommandBufferRing() override;
    virtual Ref<Command> retrieve_command() override;

    virtual void add_command(Ref<Command> p_command) override;
    // TODO
private:
};

}

#endif
