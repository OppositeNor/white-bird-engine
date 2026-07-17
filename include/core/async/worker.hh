/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef WBE_FILE_WORKER_HH
#define WBE_FILE_WORKER_HH

#include "core/core_utils.hh"
#include "core/job/job_buffer_ring_mpsc.hh"
#include "core/job/job_handler.hh"
#include "core/memory/unique.hh"
#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include "utils/interface/i_singleton.hh"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class Worker
 * @brief For handling tasks that are expensive, such as an IO task.
 *
 */
class Worker final : public ISingleton<Worker>, public JobHandler {
public:
    /**
     * @brief Maximum number of tasks that the worker could handle.
     */
    static constexpr size_t WORKER_MAX_TASK = 128;

    WBE_R6_NDCD_DELETE_COPY_MOVE(Worker);

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator to allocate
     */
    Worker() : allocator(WBE_MiB(0.5)) {
        singleton = this;
        job_buffer = make_unique<JobBufferRingMPSC>(&allocator, &allocator, WORKER_MAX_TASK);
    }

    virtual ~Worker() override {
        // Ensures the job buffer destructs before the allocator.
        job_buffer.reset();
        singleton = nullptr;
    }

    virtual void add_job(std::function<void()> p_job) override;

    static Worker* get_singleton() {
        return singleton;
    }

    /**
     * @brief Execute the worker thread.
     */
    void run();

    /**
     * @brief Read data from a file.
     *
     * @param p_promise The promise to retrieve result.
     * @param p_path The path to the file to read.
     */
    void read_file(std::promise<std::vector<char>>& p_promise, const Path& p_path) {
        read_binary_file(p_promise, p_path);
    }

    /**
     * @brief Read a text file.
     *
     * @param p_promise The promise to retrieve result.
     * @param p_path The path to the text file.
     */
    void read_text_file(std::promise<std::string>& p_promise, const Path& p_path);

    /**
     * @brief Read a binary file.
     *
     * @tparam T The type of the file's data.
     * @param p_promise The promise to retrieve result.
     * @param p_path The path to the file.
     */
    template <typename T = uint8_t>
    void read_binary_file(std::promise<std::vector<T>>& p_promise, const Path& p_path) {
        add_job([&]() {
            p_promise.set_value(load_binary_file<T>(static_cast<std::string>(p_path).c_str()));
        });
    }

    /**
     * @brief Stop.
     */
    void stop() {
        add_job([&]() {
            should_exit = true;
        });
    }

private:
    inline static Worker* singleton = nullptr;
    HeapAllocatorDefault allocator;
    Unique<JobBufferRingMPSC> job_buffer;
    bool should_exit = false;
};

} // namespace WhiteBirdEngine
#endif
