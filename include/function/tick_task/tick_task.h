#ifndef __WBE_FRAME_H__
#define __WBE_FRAME_H__

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>

namespace WhiteBirdEngine {

// TODO: test
class TickTask {
public:
    TickTask(bool p_reset_on_finish) : reset_on_finish(p_reset_on_finish), prev_task_total(0), prev_task_finish_count(0) {}
    virtual ~TickTask() {
        for (auto task : next_tasks) {
            --task->prev_task_total;
        }
    }

    virtual void init() = 0;
    virtual void update(double p_delta) = 0;

    void begin(double p_delta) {
        // TODO: block until ready
        update(p_delta);
        finish();
        if (reset_on_finish) {
            reset();
        }
    }

    bool is_ready() const {
        return prev_task_finish_count.load(std::memory_order_acquire) >= prev_task_total;
    }

    void reset() {
        prev_task_finish_count.store(0);
    }

    void add_next_task(std::shared_ptr<TickTask> p_next_task) {
        next_tasks.insert(p_next_task);
        ++p_next_task->prev_task_total;
    }

    bool is_reset_on_finish() const {
        return reset_on_finish;
    }

    void set_reset_on_finish(bool p_reset_on_finish) {
        reset_on_finish = p_reset_on_finish;
    }

private:
    bool reset_on_finish;
    uint32_t prev_task_total;
    std::atomic<uint32_t> prev_task_finish_count;
    std::set<std::shared_ptr<TickTask>> next_tasks;

    void finish() {
        for (auto next_task : next_tasks) {
            next_task->prev_task_finish_count.fetch_add(1, std::memory_order_acq_rel);
        }
    }

};

}

#endif
