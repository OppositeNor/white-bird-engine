// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
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
