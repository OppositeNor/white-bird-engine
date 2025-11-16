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
#ifndef __WBE_TAKS_BUFFER_HH__
#define __WBE_TAKS_BUFFER_HH__

#include "core/task/task.hh"
#include "core/memory/reference_strong.hh"
namespace WhiteBirdEngine {

/**
 * @class TaskBuffer
 * @brief Stores a buffer that contians all the task for an instance to execute.
 */
class TaskBuffer {
public:
    TaskBuffer() = default;
    virtual ~TaskBuffer() {}

    /**
     * @brief Retrieve a task from a buffer. If the buffer is empty, return MEM_NULL.
     */
    virtual Ref<Task> retrieve_task() = 0;

    /**
     * @brief Add a task to the buffer.
     * 
     * @throws std::runtime_error If buffer overflow.
     * @param p_task The task to add to the buffer.
     */
    virtual void add_task(Ref<Task> p_task) = 0;
};

}

#endif
