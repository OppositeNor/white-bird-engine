#ifndef __PE_GAME_H__
#define __PE_GAME_H__

#include "utils/tick_task/tick_task.h"
#include <memory>

namespace PencilEngine {

class Game {
public:
    Game() = default;
    virtual ~Game() {}

    virtual void start() = 0;
    virtual void tick(double p_delta) = 0;
    virtual void terminate() = 0;
    virtual void register_frame(std::shared_ptr<Frame> p_frame) = 0;
    

};

}

#endif
