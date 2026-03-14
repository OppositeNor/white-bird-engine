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

#include "core/job/job_buffer_ring_mpsc.hh"
#include "core/job/job_handler.hh"
#include "promise.hh"
#include "core/core_utils.hh"
#include "global/stl_allocator.hh"
#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include "utils/interface/i_singleton.hh"
#include <cstdint>
#include <string>

namespace WhiteBirdEngine {

/**
 * @class Worker
 * @brief For handling tasks that are expensive, such as an IO task.
 *
 */
class Worker : public ISingleton<Worker>, public JobHandler {
public:

    static constexpr size_t WORKER_MAX_TASK = 32;

    WBE_R6_NDC_DELETE_COPY_MOVE(Worker);

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator to allocate 
     */
    Worker(HeapAllocatorDefault* p_allocator)
        : allocator(p_allocator), job_buffer(p_allocator, WORKER_MAX_TASK) {}

    /**
     * @brief Read data from a file.
     *
     * @param p_path 
     */
    Promise<Vector<uint8_t>> read_file(const Path& p_path) {
        return read_binary_file(p_path);
    }

    /**
     * @brief Read a text file.
     *
     * @param p_path The path to the text file.
     */
    Promise<std::string> read_text_file(const Path& p_path);

    /**
     * @brief Read a binary file.
     *
     * @param p_path The path to the file.
     */
    Promise<Vector<uint8_t>> read_binary_file(const Path& p_path);

private:
    HeapAllocatorDefault* allocator;
    JobBufferRingMPSC job_buffer;
};

} // namespace WhiteBirdEngine
#endif

