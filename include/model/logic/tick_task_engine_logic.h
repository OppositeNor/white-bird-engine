#ifndef __WBE_TICK_TASK_ENGINE_LOGIC_H__
#define __WBE_TICK_TASK_ENGINE_LOGIC_H__

#include "utils/tick_task/tick_task.h"

namespace WhiteBirdEngine {

class TickTaskEngineLogic : public TickTask {
public:
    // TODO: Constructor
    virtual ~TickTaskEngineLogic() override;

    virtual void init() override;
    virtual void update(double p_delta) override;

private:
    // TODO: fields
};

}

#endif
