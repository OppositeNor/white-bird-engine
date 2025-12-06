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
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_utils.hh"

namespace WhiteBirdEngine {

ResourceManager::ResourceManager(size_t p_alloc_size, const std::string& p_global_res_path)
    : stack_allocator(p_alloc_size) {
    singleton = this;
    global_res_chunk_id = register_chunk(make_ref<ResourceChunkLoadFile<ParserJSON>>(
        global_allocator(), get_resource_path("res_chunks/global_resources.json"), stack_allocator));
}

ResourceManager::~ResourceManager() {
    singleton = nullptr;
    resource_chunks.clear();
    stack_allocator.clear();
}

Ref<ResourceChunk> ResourceManager::find_resource_chunk_containing_key(const std::string& p_key) const {
    for (const Ref<ResourceChunk>& chunk : resource_chunks) {
        if (chunk->contains(p_key)) {
            return chunk;
        }
    }
    throw std::runtime_error(std::format("Failed to find resource chunk containing key: {}.", p_key));
}

}
