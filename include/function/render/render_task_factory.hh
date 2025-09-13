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
#ifndef __WBE_RENDER_TASK_FACTORY_HH__
#define __WBE_RENDER_TASK_FACTORY_HH__

#include "core/memory/reference_strong.hh"
#include "function/render/render_task.hh"
namespace WhiteBirdEngine {

/**
 * @brief Render task factory.
 *
 * @tparam Child The child class.
 */
template <typename Child>
class RenderTaskFactory {
public:
    template <typename... Args>
    Ref<RenderTask> create(const char* p_task_name, Args&& ...p_args);
};

}


#endif
