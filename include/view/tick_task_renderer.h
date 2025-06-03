#ifndef __WBE_TICK_TASK_RENDER_H__
#define __WBE_TICK_TASK_RENDER_H__

#include "utils/tick_task/tick_task.h"

namespace WhiteBirdEngine {
class TickTaskRenderer : public TickTask {
public:
    virtual void init() override;
    virtual void update(double p_delta) override;

private:
    // TODO: renderers
};
}

#endif
