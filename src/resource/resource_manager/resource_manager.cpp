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
#include "resource/resource_manager/resource_manager.hh"
#include <stdexcept>

namespace WhiteBirdEngine {

Ref<ResourceChunkLoadFile> ResourceManager::get_resource_chunk(ResourceChunkID p_chunk_id) {
    for (auto& chunk : resource_chunks) {
        if (chunk.first == p_chunk_id) {
            return chunk.second;
        }
    }
    throw std::runtime_error("Failed to get resource from chunk: " + std::to_string(p_chunk_id));
}

}
