#include "resource/engine_resource.hh"
#include "core/engine_core.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_chunk/resource_chunk.hh"

namespace WhiteBirdEngine {

EngineResource::EngineResource() {
    resource_manager = make_unique<ResourceManager>(EngineCore::get_singleton()->pool_allocator, WBE_MiB(0.5), "res_chunks/global_resources.json");
}

}
