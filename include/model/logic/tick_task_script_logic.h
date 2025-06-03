#ifndef __WBE_TICK_TASK_SCRIPT_LOGIC_H__
#define __WBE_TICK_TASK_SCRIPT_LOGIC_H__
#include "utils/tick_task/tick_task.h"

namespace WhiteBirdEngine {
class TickTaskScriptLogic : public TickTask {
public:
    // TODO: Constructor
    virtual ~TickTaskScriptLogic() override;

    virtual void init() override;
    virtual void update(double p_delta) override;

private:
    // TODO: fields
};

}

#endif
