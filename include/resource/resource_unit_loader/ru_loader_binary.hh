// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_RU_LOADER_BINARY_HH__
#define __WBE_RU_LOADER_BINARY_HH__

#include "resource/resource_unit/resource_unit.hh"
#include "resource/resource_unit_loader/resource_unit_loader.hh"

namespace WhiteBirdEngine {

class RULoaderBinary : public ResourceUnitLoader<RULoaderBinary> {
public:
    RULoaderBinary() = default;
    ~RULoaderBinary() {}
    RULoaderBinary(const RULoaderBinary&) = delete;
    RULoaderBinary(RULoaderBinary&&) = delete;
    RULoaderBinary& operator=(const RULoaderBinary&)= delete;
    RULoaderBinary& operator=(RULoaderBinary&&) = delete;

    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        // TODO: Test
        std::string key = p_parser_data.template get_value<std::string>("key");
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));
        RUBinary ru_binary{};
        std::vector<char> str = load_binary_file(static_cast<std::string>(path).c_str());
        ru_binary.size = str.size();
        MemID data_id = p_chunk.push_data(ru_binary.size, str.data(), p_allocator);
        ru_binary.data = data_id;
        return p_chunk.push_res_unit(key, p_allocator, ru_binary);
    }
};

}

#endif
