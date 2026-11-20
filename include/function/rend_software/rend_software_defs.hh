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
#ifndef WBE_FILE_REND_SOFTWARE_DEFS
#define WBE_FILE_REND_SOFTWARE_DEFS

#include "core/alloc/stl_alloc.hh"
#include "function/render/rend_defs.hh"
namespace WhiteBirdEngine {

struct BaseRendDataSoftware {
    BaseRendDataSoftware(RendAlloc* p_alloc) : main_alloc(p_alloc), per_thread_allocs(p_alloc) {
    }
    RendAlloc* main_alloc = nullptr;
    Vector<RendAlloc*, RendAlloc> per_thread_allocs;
};

} // namespace WhiteBirdEngine
#endif
