#ifndef __WBE_ENGINE_RESOURCE_HH__
#define __WBE_ENGINE_RESOURCE_HH__

#include "core/allocator/allocator.hh"
#include "core/memory/unique.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include "utils/interface/singleton.hh"
namespace WhiteBirdEngine {

/**
 * @class EngineResource
 * @brief Engine resource singleton.
 *
 */
class EngineResource : public Singleton<EngineResource> {
public:
    EngineResource();
    virtual ~EngineResource() override;

    Unique<ResourceManager> resource_manager = MEM_NULL;
};

}

#endif
