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
#ifndef __WBE_RENDER_DEFS_HH__
#define __WBE_RENDER_DEFS_HH__

#ifdef WBE_REFLECTION_PARSER

__attribute__((annotate("WBE_DEFINE_RENDER_TASK")))
void define_wbe_render_task(const char* task_name, const char* render_task_vk) {}

__attribute__((annotate("WBE_DEFINE_RENDER_PIPELINE")))
void define_wbe_render_pipeline(const char* pipeline_name, ...)

// Define a render task.
#define WBE_RENDER_TASK(task_name, RenderTaskVK) define_wbe_render_task(#task_name, #RenderTaskVK)
// Define a render pipeline.
#define WBE_RENDER_PIPELINE(pipeline_name, ...) define_wbe_render_pipeline(#pipeline_name, __VA_ARGS__)

#else

// Define a render task.
#define WBE_RENDER_TASK(task_name, RenderTaskVK)
// Define a render pipeline.
#define WBE_RENDER_PIPELINE(...)

#endif



#endif
