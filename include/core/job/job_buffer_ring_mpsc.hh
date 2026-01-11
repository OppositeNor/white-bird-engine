#ifndef WBE_FILE_JOB_BUFFER_RING_MPSC_HH
#define WBE_FILE_JOB_BUFFER_RING_MPSC_HH

#include "core/core_utils.hh"
#include "core/job/job_buffer.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <stdexcept>
namespace WhiteBirdEngine {

/**
 * @class JobBufferRingMPSC
 *
 * @tparam JobT The type of the job.
 * @brief Job buffer, mpsc ring buffer version.
 *
 */
template <typename JobT>
class JobBufferRingMPSC final : public JobBuffer<JobBufferRingMPSC<JobT>, JobT> {
public:
    using JobType = JobT;

    JobBufferRingMPSC() = delete;
    virtual ~JobBufferRingMPSC() = default;
    WBE_R6_NDCD_DELETE_COPY_MOVE(JobBufferRingMPSC)

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator this buffer uses.
     * @param p_buffer_size The size of the buffer.
     */
    JobBufferRingMPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size);

    Ref<JobType> retrieve_job(bool p_block = false);
    void add_job(Ref<JobType> p_job);

    /**
     * @brief Get the semaphore.
     *
     * @return The semaphore.
     */
    std::counting_semaphore<32>& get_semaphore() {
        return semaphore;
    }

private:
    std::counting_semaphore<32> semaphore{0};
    Vector<Ref<JobType>> buffer;
    WBE_NO_FALSE_SHARING std::atomic<size_t> head;
    WBE_NO_FALSE_SHARING std::atomic<size_t> tail;
};

template <typename JobT>
JobBufferRingMPSC<JobT>::JobBufferRingMPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size)
    : buffer(p_allocator), head(0), tail(0) {
    if (p_buffer_size <= 1) {
        throw std::runtime_error("Buffer has to be at least size 2.");
    }
    buffer.resize(p_buffer_size);
}

template <typename JobType>
void JobBufferRingMPSC<JobType>::add_job(Ref<JobType> p_job) {
    size_t head_l;
    size_t next;

    // Use CAS for lock free increment.
    while (true) {
        head_l = head.load(std::memory_order_relaxed);
        next = ring_increment(head_l, buffer.size());
        if (next == tail.load(std::memory_order_acquire)) {
            throw std::runtime_error("Buffer overflow.");
        }
        if (head.compare_exchange_weak(
                head_l,
                next,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            break;
        }
    }
    buffer[head_l] = p_job;
    std::atomic_thread_fence(std::memory_order_release);
    semaphore.release();
}

template <typename JobType>
Ref<JobType> JobBufferRingMPSC<JobType>::retrieve_job(bool p_block) {
    if (p_block) {
        semaphore.acquire();
    }
    else if (!semaphore.try_acquire()) {
        return MEM_NULL;
    }
    size_t tail_l = tail.load(std::memory_order_acquire);

    Ref<JobType> result = buffer[tail_l];
    tail.store(ring_increment(tail_l, buffer.size()), std::memory_order_release);
    return result;
}

} // namespace WhiteBirdEngine
#endif
