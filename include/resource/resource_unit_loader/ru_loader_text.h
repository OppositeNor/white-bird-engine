#ifndef __WBE_RU_LOADER_TEXT_H__
#define __WBE_RU_LOADER_TEXT_H__

#include "core/allocator/allocator.h"
#include "resource/resource_chunk/resource_chunk.h"
#include "resource/resource_unit/ru_text.h"
#include "resource/resource_unit_loader/resource_unit_loader.h"
#include <fstream>
namespace WhiteBirdEngine {

/**
 * @class RULoaderText
 * @brief Resource unit loader for text resources.
 *
 */
class RULoaderText : public ResourceUnitLoader<RULoaderText> {
public:
    RULoaderText() = default;
    ~RULoaderText() {}
    RULoaderText(const RULoaderText&) = delete;
    RULoaderText(RULoaderText&&) = delete;
    RULoaderText& operator=(const RULoaderText&)= delete;
    RULoaderText& operator=(RULoaderText&&) = delete;

    // TODO: Test
    template <ResourceChunkConcept ResChunkType, ParserDataConcept ParserDataType>
    MemID load(ResourceChunk<ResChunkType>& p_chunk, const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
        std::string key = p_parser_data.template get_value<std::string>("key");
        ParserData<ParserDataType>& properties = p_parser_data.template get_value<ParserDataType>("properties");
        Path path = get_resource_path(p_parser_data.template get_value<std::string>("path"));

        RUText ru_text{};
        std::fstream file(static_cast<std::string>(path), std::ios::binary);
        file.seekg(0, std::ios::end);
        ru_text.size = file.tellg();
        
        return MEM_NULL;
    }
};


}

#endif
