#include "core/parser/parser.h"
#include "resource/resource_chunk/resource_chunk.h"
#include "resource/resource_unit_loader/ru_loader_image.h"
#include "resource/resource_unit_loader/ru_loader_text.h"
#include <cstring>
#include <unordered_map>

namespace WhiteBirdEngine {

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
class ResourceChunkLoadFile : public ResourceChunk<ResourceChunkLoadFile> {
public:
    virtual ~ResourceChunkLoadFile() {}
    ResourceChunkLoadFile(const ResourceChunkLoadFile&) = delete;
    ResourceChunkLoadFile(ResourceChunkLoadFile&&) = delete;
    ResourceChunkLoadFile& operator=(const ResourceChunkLoadFile&) = delete;
    ResourceChunkLoadFile& operator=(ResourceChunkLoadFile&&) = delete;

    /**
     * @brief Constructor
     *
     * @param p_parser The parser to get resource from.
     */
    template <ParserConcept ParserType>
    ResourceChunkLoadFile(const ParserType& p_parser);

    virtual MemID get_resource(const std::string& p_key) const {
        return resource_map.at(p_key);
    }

    size_t get_size() const {
        return total_size;
    }

    template <ResourceUnitConcept T>
    MemID push_res_unit(const std::string& p_res_key, StackAllocator& p_allocator, const T& p_res) {
        using PureT = std::remove_cvref<T>;
        total_size += sizeof(PureT);
        MemID result = p_allocator.allocate(sizeof(PureT));
        *(p_allocator.get_obj<PureT>(result)) = p_res;
        resource_map[p_res_key] = result;
        return result;
    }

    MemID push_data(size_t p_res_size, void* p_res, StackAllocator& p_allocator) {
        total_size += p_res_size;
        MemID result = p_allocator.allocate(p_res_size);
        memcpy(p_allocator.get(result), p_res, p_res_size);
        return result;
    }

private:
    size_t total_size;
    std::unordered_map<std::string, MemID> resource_map;

    template <ParserDataConcept ParserDataType>
    void load_ru_image(const ParserDataType& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_audio(const ParserDataType& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_video(const ParserDataType& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_text(const ParserDataType& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_font(const ParserDataType& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_binary(const ParserDataType& p_parser_data, StackAllocator& p_allocator);
};


template <ParserConcept ParserType>
ResourceChunkLoadFile::ResourceChunkLoadFile(const ParserType& p_parser) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_image(const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
    RULoaderImage loader;
    loader.template load<ParserDataType>(*this, p_parser_data);
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_audio(const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_video(const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_text(const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
    RULoaderText data_loader;
    data_loader.template load<ParserDataType>(*this, p_parser_data.get_data());
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_font(const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_binary(const ParserDataType& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

}
