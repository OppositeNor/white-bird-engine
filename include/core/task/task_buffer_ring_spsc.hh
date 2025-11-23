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
#ifndef __WBE_TAKS_BUFFER_RING_SPSC_HH__
#define __WBE_TAKS_BUFFER_RING_SPSC_HH__

#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/task/task.hh"
#include "global/stl_allocator.hh"
#include "task_buffer.hh"
#include <cstddef>

namespace WhiteBirdEngine {

/**
 * @class TaskBufferRingSPSC
 * @brief Task buffer, spsc ring buffer version.
 *
 */
class TaskBufferRingSPSC final : public TaskBuffer {
public:
    virtual ~TaskBufferRingSPSC() override {}
    TaskBufferRingSPSC(const TaskBufferRingSPSC&) = delete;
    TaskBufferRingSPSC(TaskBufferRingSPSC&&) = delete;
    TaskBufferRingSPSC& operator=(const TaskBufferRingSPSC&) = delete;
    TaskBufferRingSPSC& operator=(TaskBufferRingSPSC&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param p_allocator The allocator this buffer uses.
     * @param p_buffer_size The size of the buffer.
     */
    TaskBufferRingSPSC(HeapAllocatorDefault* p_allocator, size_t p_buffer_size);

    virtual Ref<Task> retrieve_task() override;
    virtual void add_task(Ref<Task> p_task) override;

private:
    vector<Ref<Task>> buffer;
    WBE_NO_FALSE_SHARING std::atomic<size_t> head;
    WBE_NO_FALSE_SHARING std::atomic<size_t> tail;
};

}

#endif
