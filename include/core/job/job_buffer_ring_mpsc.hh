#ifndef WBE_FILE_JOB_BUFFER_RING_MPSC_HH
#define WBE_FILE_JOB_BUFFER_RING_MPSC_HH

#include "core/core_utils.hh"
#include "core/job/job_buffer.hh"
#include "global/stl_allocator.hh"
#include "utils/defs.hh"
namespace WhiteBirdEngine {

/**
 * @class JobBufferRingMPSC
 *
 * @tparam JobT The type of the job.
 * @brief Job buffer, mpsc ring buffer version.
 *
 */
class JobBufferRingMPSC final : public JobBuffer<JobBufferRingMPSC> {
public:

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

    Ref<Job> retrieve_job(bool p_block = false);
    void add_job(Ref<Job> p_job);

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
    Vector<Ref<Job>> buffer;
    WBE_NO_FALSE_SHARING std::atomic<size_t> head;
    WBE_NO_FALSE_SHARING std::atomic<size_t> tail;
};

} // namespace WhiteBirdEngine
#endif
