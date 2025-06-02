#ifndef __PE_FRAME_H__
#define __PE_FRAME_H__

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace PencilEngine {

// TODO: test
class TickTask {
public:
    TickTask() : semaphore_count(0), semaphore_remain(0) {}

    TickTask(uint32_t p_semaphore_count, bool p_is_start_ready = false)
        : semaphore_count(p_semaphore_count), semaphore_remain(p_is_start_ready ? 0 : p_semaphore_count) {}

    virtual void init() = 0;
    virtual void update(double p_delta) = 0;

    void tick(double p_delta) {
        update(p_delta);
        finish();
    }

    bool is_ready() const {
        return semaphore_remain <= 0;
    }

    void reset_semaphores() {
        semaphore_remain = semaphore_count;
    }

    void add_next_semaphore(std::shared_ptr<TickTask> p_next_semaphore) {
        next_semaphores.push_back(p_next_semaphore);
        p_next_semaphore->add_total_semaphore_count();
    }

private:
    uint32_t semaphore_count;
    std::atomic<int> semaphore_remain;
    std::vector<std::shared_ptr<TickTask>> next_semaphores;

    void add_total_semaphore_count() {
        ++semaphore_count;
    }

    void finish() {
        for (auto next_semaphore : next_semaphores) {
            --(next_semaphore->semaphore_remain);
        }
    }

};

}

#endif
