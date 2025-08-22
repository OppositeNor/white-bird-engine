// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_RU_LOADER_MESH_HH__
#define __WBE_RU_LOADER_MESH_HH__

#include "resource/resource_unit_loader/resource_unit_loader.hh"

namespace WhiteBirdEngine {

/**
 * @class RULoaderMesh
 * @brief Resource unit loader for hdr images.
 *
 */
class RULoaderMesh : public ResourceUnitLoader<RULoaderMesh> {
public:
    RULoaderMesh() = default;
    ~RULoaderMesh() {}
    RULoaderMesh(const RULoaderMesh&) = delete;
    RULoaderMesh(RULoaderMesh&&) = delete;
    RULoaderMesh& operator=(const RULoaderMesh&)= delete;
    RULoaderMesh& operator=(RULoaderMesh&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        return MEM_NULL; // TODO
    }

};


}

#endif

