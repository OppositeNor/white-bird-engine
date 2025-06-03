#ifndef __WBE_TICK_TASK_ANIMATION_H__
#define __WBE_TICK_TASK_ANIMATION_H__

#include "utils/tick_task/tick_task.h"
namespace WhiteBirdEngine {

class TickTaskAnimation : public TickTask {
public:
    // TODO: Constructor
    virtual ~TickTaskAnimation() override;

    virtual void init() override;
    virtual void update(double p_delta) override;

private:
    // TODO: fields
    
};

}

#endif
