#ifndef __TICK_TASK_LOGIC_H__
#define __TICK_TASK_LOGIC_H__
#include "utils/tick_task/tick_task.h"

namespace PencilEngine {
class TickTaskScriptLogic : public TickTask {
public:
    // TODO: Constructor
    TickTaskScriptLogic() = default;
    virtual ~TickTaskScriptLogic() override;

    virtual void init() override;
    virtual void update(double p_delta) override;

private:
    // TODO: fields
};

}

#endif
