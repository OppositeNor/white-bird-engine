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
#include "core/allocator/heap_allocator_ram.hh"
#include "core/allocator/allocator.hh"
#include "core/logging/log.hh"

namespace WhiteBirdEngine {

HeapAllocatorRAM::~HeapAllocatorRAM() {
    if (allocated.size() != 0) {
        wbe_console_log(WBE_CHANNEL_GLOBAL)->warning("HeapAllocatorRAM not empty during destruction.");
    }
    for (auto& iter : allocated) {
        free(get(iter));
    }
}

MemID HeapAllocatorRAM::allocate(size_t p_size) {
    if (p_size == 0) {
        return MEM_NULL;
    }
    MemID result = std::bit_cast<MemID>(malloc(p_size));
    allocated.insert(result);
    return result;
}

void HeapAllocatorRAM::deallocate(MemID p_mem) {
    auto iter = allocated.find(p_mem);
    if (iter == allocated.end()) {
        throw std::runtime_error("Failed to deallocate memory: memory not allocated by this allocator.");
    }
    free(get(p_mem));
    allocated.erase(p_mem);
}


}
